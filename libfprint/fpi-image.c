/*
 * FPrint Image - Private APIs
 * Copyright (C) 2007 Daniel Drake <dsd@gentoo.org>
 * Copyright (C) 2019 Benjamin Berg <bberg@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#define FP_COMPONENT "image"

#include "fpi-image.h"
#include "fpi-log.h"

#include <nbis.h>
#include <config.h>

#ifdef HAVE_PIXMAN
#include <pixman.h>
#endif

/**
 * SECTION: fpi-image
 * @title: Internal FpImage
 * @short_description: Internal image handling routines
 *
 * Internal image handling routines. See #FpImage for public routines.
 */

/**
 * fpi_std_sq_dev:
 * @buf: buffer (usually bitmap, one byte per pixel)
 * @size: size of @buffer
 *
 * Calculates the squared standard deviation of the individual
 * pixels in the buffer, as per the following formula:
 * |[<!-- -->
 *    mean = sum (buf[0..size]) / size
 *    sq_dev = sum ((buf[0.size] - mean) ^ 2)
 * ]|
 * This function is usually used to determine whether image
 * is empty.
 *
 * Returns: the squared standard deviation for @buffer
 */
gint
fpi_std_sq_dev (const guint8 *buf,
                gint          size)
{
  guint64 res = 0, mean = 0;
  gint i;

  for (i = 0; i < size; i++)
    mean += buf[i];

  mean /= size;

  for (i = 0; i < size; i++)
    {
      int dev = (int) buf[i] - mean;
      res += dev * dev;
    }

  return res / size;
}

/**
 * fpi_mean_sq_diff_norm:
 * @buf1: buffer (usually bitmap, one byte per pixel)
 * @buf2: buffer (usually bitmap, one byte per pixel)
 * @size: buffer size of smallest buffer
 *
 * This function calculates the normalized mean square difference of
 * two buffers, usually two lines, as per the following formula:
 * |[<!-- -->
 *    sq_diff = sum ((buf1[0..size] - buf2[0..size]) ^ 2) / size
 * ]|
 *
 * This functions is usually used to get numerical difference
 * between two images.
 *
 * Returns: the normalized mean squared difference between @buf1 and @buf2
 */
gint
fpi_mean_sq_diff_norm (const guint8 *buf1,
                       const guint8 *buf2,
                       gint          size)
{
  int res = 0, i;

  for (i = 0; i < size; i++)
    {
      int dev = (int) buf1[i] - (int) buf2[i];
      res += dev * dev;
    }

  return res / size;
}

FpImage *
fpi_image_resize (FpImage *orig_img,
                  guint    w_factor,
                  guint    h_factor)
{
#ifdef HAVE_PIXMAN
  int new_width = orig_img->width * w_factor;
  int new_height = orig_img->height * h_factor;
  pixman_image_t *orig, *resized;
  pixman_transform_t transform;
  FpImage *newimg;

  orig = pixman_image_create_bits (PIXMAN_a8, orig_img->width, orig_img->height, (uint32_t *) orig_img->data, orig_img->width);
  resized = pixman_image_create_bits (PIXMAN_a8, new_width, new_height, NULL, new_width);

  pixman_transform_init_identity (&transform);
  pixman_transform_scale (NULL, &transform, pixman_int_to_fixed (w_factor), pixman_int_to_fixed (h_factor));
  pixman_image_set_transform (orig, &transform);
  pixman_image_set_filter (orig, PIXMAN_FILTER_BILINEAR, NULL, 0);
  pixman_image_composite32 (PIXMAN_OP_SRC,
                            orig, /* src */
                            NULL, /* mask */
                            resized, /* dst */
                            0, 0, /* src x y */
                            0, 0, /* mask x y */
                            0, 0, /* dst x y */
                            new_width, new_height /* width height */
                           );

  newimg = fp_image_new (new_width, new_height);
  newimg->flags = orig_img->flags;

  memcpy (newimg->data, pixman_image_get_data (resized), new_width * new_height);

  pixman_image_unref (orig);
  pixman_image_unref (resized);

  return newimg;
#else
  fp_err ("Libfprint compiled without pixman support, impossible to resize");

  return g_object_ref (orig_img);
#endif
}

/**
 * fpi_image_enhance:
 * @image: a #FpImage
 * @error: (out)(optional): return location for an error
 *
 * Enhance fingerprint image quality using block contrast normalization
 * and unsharp masking.
 *
 * Block contrast normalization divides the image into 64x64 pixel blocks
 * and applies contrast stretching within each block, mapping the minimum
 * pixel value to 0 and maximum to 255. This compensates for uneven
 * illumination and pressure across the finger.
 *
 * Unsharp masking subtracts a gaussian-blurred version from the original
 * to emphasize ridge edges. Uses a 5x5 kernel with sigma ~1.0 and
 * amount 0.5.
 *
 * Returns: %TRUE on success, %FALSE on error with @error set.
 */
gboolean
fpi_image_enhance (FpImage *image,
                   GError **error)
{
  guint8 *data = image->data;
  gint w = image->width;
  gint h = image->height;
  gint x, y, bx, by;
  gint block_size = 64;
  gint n_blocks_x = (w + block_size - 1) / block_size;
  gint n_blocks_y = (h + block_size - 1) / block_size;
  g_autofree guint8 *enhanced = g_malloc (w * h);
  g_autofree guint8 *blurred = g_malloc (w * h);

  /* Block contrast normalization */
  for (by = 0; by < n_blocks_y; by++)
    {
      for (bx = 0; bx < n_blocks_x; bx++)
        {
          gint block_min = 255, block_max = 0;

          for (y = by * block_size; y < (by + 1) * block_size && y < h; y++)
            {
              for (x = bx * block_size; x < (bx + 1) * block_size && x < w; x++)
                {
                  guint8 p = data[y * w + x];
                  if (p < block_min) block_min = p;
                  if (p > block_max) block_max = p;
                }
            }

          gint range = block_max - block_min;
          if (range < 10) range = 10;

          for (y = by * block_size; y < (by + 1) * block_size && y < h; y++)
            {
              for (x = bx * block_size; x < (bx + 1) * block_size && x < w; x++)
                {
                  gint idx = y * w + x;
                  gint v = data[idx];
                  v = (v - block_min) * 255 / range;
                  if (v < 0) v = 0;
                  if (v > 255) v = 255;
                  enhanced[idx] = v;
                }
            }
        }
    }

  /* Gaussian blur (5x5, sigma ~1.0) */
  {
    gint kernel[5][5] = {
      {1, 4, 7, 4, 1},
      {4, 16, 26, 16, 4},
      {7, 26, 41, 26, 7},
      {4, 16, 26, 16, 4},
      {1, 4, 7, 4, 1},
    };
    gint kernel_sum = 273;

    for (y = 0; y < h; y++)
      {
        for (x = 0; x < w; x++)
          {
            gint sum = 0;

            for (gint ky = -2; ky <= 2; ky++)
              {
                for (gint kx = -2; kx <= 2; kx++)
                  {
                    gint px = x + kx;
                    gint py = y + ky;
                    if (px >= 0 && px < w && py >= 0 && py < h)
                      sum += enhanced[py * w + px] * kernel[ky + 2][kx + 2];
                    else
                      sum += enhanced[y * w + x] * kernel[ky + 2][kx + 2];
                  }
              }

            blurred[y * w + x] = sum / kernel_sum;
          }
      }
  }

  /* Unsharp mask: enhanced + amount * (enhanced - blurred) */
  for (y = 0; y < h; y++)
    {
      for (x = 0; x < w; x++)
        {
          gint idx = y * w + x;
          gint sharp = enhanced[idx] + 0.5 * (enhanced[idx] - blurred[idx]);
          if (sharp < 0) sharp = 0;
          if (sharp > 255) sharp = 255;
          data[idx] = sharp;
        }
    }

  return TRUE;
}
