/*
 * Python libwebp wrapper's encoder methods.
 *
 * following function refer to Skal(pascal.massimino@gmail.com)'s cwebp.c:
 *  - ReadYUV
 *  - ReadJPEG
 *  - ReadPNG
 *  - GetImageType
 *  - ReadPicture
 */
#include <stdio.h>
#include <png.h>
#include <jpeglib.h>
#include "webp/encode.h"
#include "Python.h"


static int ReadYUV(FILE* in_file, WebPPicture* const pic) {
  const int uv_width = (pic->width + 1) / 2;
  const int uv_height = (pic->height + 1) / 2;
  int y;
  int ok = 0;

  if (!WebPPictureAlloc(pic)) return ok;

  for (y = 0; y < pic->height; ++y) {
    if (fread(pic->y + y * pic->y_stride, pic->width, 1, in_file) != 1) {
      goto End;
    }
  }
  for (y = 0; y < uv_height; ++y) {
    if (fread(pic->u + y * pic->uv_stride, uv_width, 1, in_file) != 1)
      goto End;
  }
  for (y = 0; y < uv_height; ++y) {
    if (fread(pic->v + y * pic->uv_stride, uv_width, 1, in_file) != 1)
      goto End;
  }
  ok = 1;

 End:
  return ok;
}

struct my_error_mgr {
  struct jpeg_error_mgr pub;
  jmp_buf setjmp_buffer;
};

static void my_error_exit(j_common_ptr dinfo) {
  struct my_error_mgr* myerr = (struct my_error_mgr*) dinfo->err;
  (*dinfo->err->output_message) (dinfo);
  longjmp(myerr->setjmp_buffer, 1);
}

static int ReadJPEG(FILE* in_file, WebPPicture* const pic) {
  int ok = 0;
  int stride, width, height;
  uint8_t* rgb = NULL;
  uint8_t* row_ptr = NULL;
  struct jpeg_decompress_struct dinfo;
  struct my_error_mgr jerr;
  JSAMPARRAY buffer;

  dinfo.err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit = my_error_exit;

  if (setjmp (jerr.setjmp_buffer)) {
 Error:
    jpeg_destroy_decompress(&dinfo);
    goto End;
  }

  jpeg_create_decompress(&dinfo);
  jpeg_stdio_src(&dinfo, in_file);
  jpeg_read_header(&dinfo, TRUE);

  dinfo.out_color_space = JCS_RGB;
  dinfo.dct_method = JDCT_IFAST;
  dinfo.do_fancy_upsampling = TRUE;

  jpeg_start_decompress(&dinfo);

  if (dinfo.output_components != 3) {
    goto Error;
  }

  width = dinfo.output_width;
  height = dinfo.output_height;
  stride = dinfo.output_width * dinfo.output_components * sizeof(*rgb);

  rgb = (uint8_t*)malloc(stride * height);
  if (rgb == NULL) {
    goto End;
  }
  row_ptr = rgb;

  buffer = (*dinfo.mem->alloc_sarray) ((j_common_ptr) &dinfo,
                                       JPOOL_IMAGE, stride, 1);
  if (buffer == NULL) {
    goto End;
  }

  while (dinfo.output_scanline < dinfo.output_height) {
    if (jpeg_read_scanlines(&dinfo, buffer, 1) != 1) {
      goto End;
    }
    memcpy(row_ptr, buffer[0], stride);
    row_ptr += stride;
  }

  jpeg_finish_decompress (&dinfo);
  jpeg_destroy_decompress (&dinfo);

  // WebP conversion.
  pic->width = width;
  pic->height = height;
  ok = WebPPictureImportRGB(pic, rgb, stride);

 End:
  if (rgb) {
    free(rgb);
  }
  return ok;
}

static void PNGAPI error_function(png_structp png, png_const_charp dummy) {
  (void)dummy;  // remove variable-unused warning
  longjmp(png_jmpbuf(png), 1);
}

static int ReadPNG(FILE* in_file, WebPPicture* const pic) {
  png_structp png;
  png_infop info;
  int color_type, bit_depth, interlaced;
  int num_passes;
  int p;
  int ok = 0;
  png_uint_32 width, height, y;
  int stride;
  uint8_t* rgb = NULL;

  png = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
  if (png == NULL) {
    goto End;
  }

  png_set_error_fn(png, 0, error_function, NULL);
  if (setjmp(png_jmpbuf(png))) {
 Error:
    png_destroy_read_struct(&png, NULL, NULL);
    if (rgb) free(rgb);
    goto End;
  }

  info = png_create_info_struct(png);
  if (info == NULL) goto Error;

  png_init_io(png, in_file);
  png_read_info(png, info);
  if (!png_get_IHDR(png, info,
                    &width, &height, &bit_depth, &color_type, &interlaced,
                    NULL, NULL)) goto Error;

  png_set_strip_16(png);
  png_set_packing(png);
  if (color_type == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png);
  if (color_type == PNG_COLOR_TYPE_GRAY) {
    if (bit_depth < 8) {
      png_set_expand_gray_1_2_4_to_8(png);
    }
    png_set_gray_to_rgb(png);
  }
  if (png_get_valid(png, info, PNG_INFO_tRNS)) {
    png_set_tRNS_to_alpha(png);
  }

  // TODO(skal): Strip Alpha for now (till Alpha is supported).
  png_set_strip_alpha(png);
  num_passes = png_set_interlace_handling(png);
  png_read_update_info(png, info);
  stride = 3 * width * sizeof(*rgb);
  rgb = (uint8_t*)malloc(stride * height);
  if (rgb == NULL) goto Error;
  for (p = 0; p < num_passes; ++p) {
    for (y = 0; y < height; ++y) {
      png_bytep row = rgb + y * stride;
      png_read_rows(png, &row, NULL, 1);
    }
  }
  png_read_end(png, info);
  png_destroy_read_struct(&png, &info, NULL);

  pic->width = width;
  pic->height = height;
  ok = WebPPictureImportRGB(pic, rgb, stride);
  free(rgb);

 End:
  return ok;
}

typedef enum {
  PNG = 0,
  JPEG,
  UNSUPPORTED,
} InputFileFormat;

static InputFileFormat GetImageType(FILE* in_file) {
  InputFileFormat format = UNSUPPORTED;
  unsigned int magic;
  unsigned char buf[4];

  if ((fread(&buf[0], 4, 1, in_file) != 1) ||
      (fseek(in_file, 0, SEEK_SET) != 0)) {
    return format;
  }

  magic = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
  if (magic == 0x89504E47U) {
    format = PNG;
  } else if (magic >= 0xFFD8FF00U && magic <= 0xFFD8FFFFU) {
    format = JPEG;
  }
  return format;
}

static int ReadPicture(const char* const filename, WebPPicture* const pic) {
  int ok = 0;
  FILE* in_file = fopen(filename, "rb");
  if (in_file == NULL) {
    fprintf(stderr, "Error! Cannot open input file '%s'\n", filename);
    return ok;
  }

  if (pic->width == 0 || pic->height == 0) {
    // If no size specified, try to decode it as PNG/JPEG (as appropriate).
    const InputFileFormat format = GetImageType(in_file);
    if (format == PNG) {
      ok = ReadPNG(in_file, pic);
    } else if (format == JPEG) {
      ok = ReadJPEG(in_file, pic);
    }
  } else {
    // If image size is specified, infer it as YUV format.
    ok = ReadYUV(in_file, pic);
  }
  if (!ok) {
    fprintf(stderr, "Error! Could not process file %s\n", filename);
  }

  fclose(in_file);
  return ok;
}

static inline void _webp_free(WebPPicture* picture, FILE *out)
{
    free(picture->extra_info);
    WebPPictureFree(picture);
    if (out != NULL) fclose(out);
}

static int Writer(const uint8_t* data, size_t data_size,
                  const WebPPicture* const pic) {
  FILE* const out = (FILE*)pic->custom_ptr;
  return data_size ? (fwrite(data, data_size, 1, out) == 1) : 1;
}

PyObject*
webp_encode(PyObject *self, PyObject *args, PyObject *kwargs)
{
    int quality=80;
    int method=-1;
    char *input_filename, *output_filename;
    FILE *output=NULL;
    WebPPicture picture;
    WebPConfig config;

    /* TODO: not support all config option */
	static char *keywords[] = {"input", "output", "quality", "method", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "ss|ii:encode", keywords,
                                     &input_filename, &output_filename,
                                     &quality, &method)){
		return NULL;
    }

    if (!WebPPictureInit(&picture) || !WebPConfigInit(&config)) {
        fprintf(stderr, "initialized error\n");
        _webp_free(&picture, output);
        return NULL;
    }

    if (!ReadPicture(input_filename, &picture)) {
        fprintf(stderr, "ReadPicture(%s) error!!\n", input_filename);
        _webp_free(&picture, output);
        return NULL;
    }

    output = fopen(output_filename, "wb");
    if (output == NULL) {
        fprintf(stderr, "fopen(out) error!!\n");
        _webp_free(&picture, output);
        return NULL;
    }
    picture.writer = Writer;
    picture.custom_ptr = (void *)output;

    /* settting options */
    config.quality = quality;
    if (0 <= method && method <= 6)
        config.method = method;

    if (!WebPEncode(&config, &picture)) {
        fprintf(stderr, "WebPEncode() error!!\n");
        _webp_free(&picture, output);
        return NULL;
    }
    Py_RETURN_NONE;
}

static char WebPEncodeDoc[] = "Webp format encoder.\n";

static PyMethodDef WebPEncodeMethods[] = {
    {"encode", (PyCFunction)webp_encode, METH_VARARGS | METH_KEYWORDS,
     "webp encoder"},
    {NULL, NULL, 0, NULL}        /* Sentinel */
};

PyMODINIT_FUNC
initencode(void)
{
    PyObject *m;

    m = Py_InitModule3("webp.encode", WebPEncodeMethods, WebPEncodeDoc);
    if (!m)
        return;
}
