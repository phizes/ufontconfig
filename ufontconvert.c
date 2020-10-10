/*
TrueType to Adafruit_GFX font converter.  Derived from Peter Jakobs'
Adafruit_ftGFX fork & makefont tool, and Paul Kourany's Adafruit_mfGFX.

NOT AN ARDUINO SKETCH.  This is a command-line tool for preprocessing
fonts to be used with the Adafruit_GFX Arduino library.

For UNIX-like systems.  Outputs to stdout; redirect to header file, e.g.:
  ./ufontconvert ~/Library/Fonts/FreeSans.ttf 18 > FreeSans18pt7b.h

REQUIRES FREETYPE LIBRARY.  www.freetype.org

Currently this only extracts the printable 7-bit ASCII chars of a font.
Will eventually extend with some int'l chars a la ftGFX, not there yet.
Keep 7-bit fonts around as an option in that case, more compact.

See notes at end for glyph nomenclature & other tidbits.
*/
#ifndef ARDUINO

#include <ctype.h>
#include <ft2build.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>

#include FT_GLYPH_H
#include FT_MODULE_H
#include FT_TRUETYPE_DRIVER_H
#include "../gfxfont.h" // Adafruit_GFX font structures

#define DPI 141 // Approximate res. of Adafruit 2.8" TFT
#define MAXNUMGLYPHS 127
#define MAXGLYPHSIZE 64

FILE *outFile;
typedef struct {
    int x0, x1, y0, y1;
    int xOff, yOff;
    int rows, width;
    int xAdvance;
    int bitmapOffset;
    char c;
    char *pix;
} glyph_info ;

// Accumulate bits for output, with periodic hexadecimal byte write
void encbit(FILE *out, uint8_t value) {
  static uint8_t sum = 0, bit = 0x80;
  if (value)
    sum |= bit;          // Set bit if needed
  if (!(bit >>= 1)) {    // Advance to next bit, end of byte reached?
    fprintf(out," 0x%02X,", sum); // Write byte value
    sum = 0;               // Clear for next byte
    bit = 0x80;            // Reset bit counter
  }
}

/*
void printTableInfo(FILE *out, glyph_info tab) {
    fprintf(out," %c -- [(%2d,%2d)(%2d,%2d)] [%2dx%2d] %04d-%04d Of [%dx%d]", 
           tab.c, tab.x0, tab.y0, tab.x1, tab.y1, tab.width, tab.rows, 
           tab.xAdvance, tab.bitmapOffset,
           tab.xOff, tab.yOff);
}
*/

void printTableInfo(FILE *out, glyph_info tab) {
    fprintf(out," %c -- [%2dx%2d] %04d-%04d Of [%dx%d]", 
           tab.c, tab.width, tab.rows, 
           tab.xAdvance, tab.bitmapOffset,
           tab.xOff, tab.yOff);
}

void glyphTrans(glyph_info g) {
}

uint8_t max(uint8_t a, uint8_t b) {
    if (a > b) {
       return a;
    } else {
       return b;
    }
}

/*
void glyphTransform(FT_BitmapGlyphRec *old) {
     uint8_t newwidth, newheight, newpitch;
     uint8_t x,y;
     uint8_t byte, bit;
     uint8_t raster[40][40];
     uint8_t *buf;
     
    
     raster=malloc((sizeof(uint8_t) * 
        ((old->width + 2) * (old->rows + 2))) * 4);
     buf=malloc((sizeof(uint8_t) * 
        ((old->width + 2) * (old->rows + 2)) / 8 ) * 4);
      
     if ((old->width <= 40) || (old->rows <= 40)) {
     for (y = 0; y < old->rows; y++) {
       for (x = 0; x < old->width; x++) {
         byte = x / 8;
         bit = 0x80 >> (x & 7);
         raster[x+1][y+1] = 0;
         if ((old->buffer[y * old->pitch + byte] & bit) > 0) {
            raster[x+1][y+1] = 1;
         }
       }
     }
   
// transform the raster     
     newwidth = old->width+2;
     newheight = old->rows+2;
     newpitch = old->pitch;
     printf("%d %d %d .3\n", newwidth, newheight, newpitch); 
   
     for (y = 0; y < newheight; y++) {
       for (x = 0; x < newwidth; x++ ) {
          if ((x == 0) || (y == 0) || 
              (x == (newwidth-1)) || (y == (newheight-1))) 
              raster[x][y] = 1;
       }
     } 
  printf(".4\n"); 

// encode the raster into a bitmap
     for (y = 0; y < newheight; y++) {
       for (x = 0; x < newwidth; x++) {
         byte = x / 8;
         bit = 0x80 >> (x & 7);
         printf (" %d %d %d %d.4\n", x,y, y * newpitch + byte, bit);
         if (raster[x][y] == 0) {
             buf[y * newpitch + byte] = 
                buf[y * newpitch + byte] & (255 - bit);
         } else {
             buf[y * newpitch + byte] = 
                buf[y * newpitch + byte] | bit;
         }
       }
     } 
  printf(".5\n"); 
     new->width = newwidth;
  printf(".5\n"); 
     new->rows = newheight;
  printf(".5\n"); 
     new->pitch = newpitch;
  printf(".5\n"); 
     new->buffer = buf;    
  printf(".5\n"); 
//     FT_Glyph_Done(old);
     old = new;
  }
  printf(".6\n"); 

}
//
*/

int main(int argc, char *argv[]) {
  int i, j, err, size, first = ' ', last = '~', row = 0, col = 0,
      bitmapOffset = 0, x = 0, y = 0, fontrun = 0, pen_x = 0, pen_y = 0,
      tex_width = 0, tex_height = 0, max_dim = 0,
      charheight = 0, maxcharheight = 0, maxcharwidth = 0, maxgtop = 0,
      byte = 0, bit = 0, bitwritten = 0, glyphwritten= 0;
  long int memsize = 0;
  char c;
  char *fontName, *pixels, *ptr, *fileName;
  FT_Library library;
  FT_Face face;
  FT_Glyph glyph;
  FT_Bitmap *bmp;
  glyph_info table[MAXNUMGLYPHS];

//  FT_Glyph glyph;
//  FT_BitmapGlyphRec *g;
//  GFXglyph *table; // will be malloced
//  struct glyph_info table[MAXNUMGLYPHS];
  
//  struct glyph_info {
//    int x0,x1,y0,y1
//    int xOffset, yOffset,
//    int height, width,
//    int xAdvance;
//    int bitmapOffset;
//    char **pix;
//  } table[MAXNUMGLYPHS];

  // Parse command line.  Valid syntaxes are:
  //   ufontconvert [filename] [size]
  //   ufontconvert [filename] [size] [last char]
  //   ufontconvert [filename] [size] [first char] [last char]
  // Unless overridden, default first and last chars are
  // ' ' (space) and '~', respectively

  if (argc < 3) {
    fprintf(stderr, "Usage: %s fontfile size [first] [last]\n", argv[0]);
    return 1;
  }

  size = atoi(argv[2]);

  if (argc == 4) {
    last = atoi(argv[3]);
  } else if (argc == 5) {
    first = atoi(argv[3]);
    last = atoi(argv[4]);
  }

  if (last < first) {
    i = first;
    first = last;
    last = i;
  }
  
  if ((last - first) > MAXNUMGLYPHS) {
    fprintf(stderr, "Maximum number of glyphs is %d\n", MAXNUMGLYPHS);
    return 1;
  }

  ptr = strrchr(argv[1], '/'); // Find last slash in filename
  if (ptr)
    ptr++; // First character of filename (path stripped)
  else
    ptr = argv[1]; // No path; font in local dir.

  // Allocate space for font name and glyph table
  if (!(fontName = malloc(strlen(ptr) + 20))) {
    fprintf(stderr, "Malloc error\n");
    return 1;
  }
  memsize += strlen(ptr) + 20;

  if (!(fileName = malloc(strlen(ptr) + 20))) {
    fprintf(stderr, "Malloc error\n");
    return 1;
  }
  memsize += strlen(ptr) + 20;

  // Derive font table names from filename.  Period (filename
  // extension) is truncated and replaced with the font size & bits.
  strcpy(fontName, ptr);
  ptr = strrchr(fontName, '.'); // Find last period (file ext)
  if (!ptr)
    ptr = &fontName[strlen(fontName)]; // If none, append
  // Insert font size and 7/8 bit.  fontName was alloc'd w/extra
  // space to allow this, we're not sprintfing into Forbidden Zone.
  sprintf(ptr, "-%dpt%db", size, (last > 127) ? 8 : 7);
  // Space and punctuation chars in name replaced w/ underscores.
  for (i = 0; (c = fontName[i]); i++) {
    if (isspace(c) || ispunct(c))
      fontName[i] = '_';
  }

  // Init FreeType lib, load font
  if ((err = FT_Init_FreeType(&library))) {
    fprintf(stderr, "FreeType init error: %d", err);
    return err;
  }

  // Use TrueType engine version 35, without subpixel rendering.
  // This improves clarity of fonts since this library does not
  // support rendering multiple levels of gray in a glyph.
  // See https://github.com/adafruit/Adafruit-GFX-Library/issues/103
  FT_UInt interpreter_version = TT_INTERPRETER_VERSION_35;
  FT_Property_Set(library, "truetype", "interpreter-version",
                  &interpreter_version);

  if ((err = FT_New_Face(library, argv[1], 0, &face))) {
    fprintf(stderr, "Font load error: %d", err);
    FT_Done_FreeType(library);
    return err;
  }

  // << 6 because '26dot6' fixed-point format
  FT_Set_Char_Size(face, size << 6, 0, DPI, 0);

  // Currently all symbols from 'first' to 'last' are processed.
  // Fonts may contain WAY more glyphs than that, but this code
  // will need to handle encoding stuff to deal with extracting
  // the right symbols, and that's not done yet.
  // fprintf(stderr, "%ld glyphs\n", face->num_glyphs);

  max_dim = (1 + (face->size->metrics.height >> 6)) * 
             ceilf(sqrtf(MAXNUMGLYPHS));
  tex_width = 1;
  while(tex_width < max_dim) tex_width <<= 1;
  tex_height = tex_width;
  
  printf("ufontconvert.c font conversion program [PhB]\n");
  printf("Text dimensions %s [%3dx%3d] \n",fontName,tex_width,tex_height);

// char* pixels = (char *)calloc(tex_width * tex_height, 1);
  pen_x = 0; 
  pen_y = 0;

  maxcharheight = 0;
  maxcharwidth = 0;
  
  //if (!(fileName = malloc(strlen(fontName) + 20))) {
  //   printf("\nCannot allocate memory for fileName\n");
  //   return errno;
  //}
  
  // Process glyphs and output huge bitmap data array
  for (i = first, j = 0; i <= last; i++, j++) {
    // MONO renderer provides clean image with perfect crop
    // (no wasted pixels) via bitmap struct.
    // FT_LOAD_RENDER || FT_LOAD_FORCE_AUTOHINT || FT_LOAD_TARGET_LIGHT
    if ((err = FT_Load_Char(face, i, FT_LOAD_TARGET_MONO))) {
      fprintf(stderr, "Error %d loading char '%c'\n", err, i);
      continue;
    }
    if ((err = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_MONO))) {
      fprintf(stderr, "Error %d rendering char '%c'\n", err, i);
      continue;
    }
    if ((err = FT_Get_Glyph(face->glyph, &glyph))) {
      fprintf(stderr, "Error %d getting glyph '%c'\n", err, i);
      continue;
    }

    bmp = &face->glyph->bitmap;
    pixels = (char *)calloc((bmp->width + 2) * (bmp->rows + 2), 1);
    memsize += sizeof(char) * ((bmp->width + 2) * (bmp->rows + 2));

    for (row = 0; row < (bmp->rows + 2); row++) { 
       for (col = 0; col < (bmp->width + 2); col++) {
          pixels[row * bmp->width + col] = 0;
       }
    }      

    if (pen_x + bmp->width >= tex_width) {
       pen_x = 0;
       pen_y = ((face->size->metrics.height >> 6) + 1);
    }
//    fprintf(outFile,".CHAR %d memsize %ld [%dx%d][%d,%d][%dx%d]\n", 
//            j,memsize,bmp->width,bmp->rows,pen_x,pen_y,tex_height,tex_width);
    for (row = 0; row < bmp->rows; row++) { 
       for (col = 0; col < bmp->width; col++) {
//          x = pen_x + col;
//          y = pen_y + row;
          x = col;
          y = row;
          byte = x / 8;
          bit = 0x80 >> (x & 7);
          
// fprintf(outFile,"pix (%d,%d)=%d %d %d (%d,%d)=%d\n", 
//        col,row,row+(col*bmp->width),byte,bit,x,y,byte+(y*bmp->pitch));
          if ((bmp->buffer[(y * bmp->pitch) + byte] & bit) == 0) { 
//             fprintf(outFile,"pix (%d,%d)=%d %d %d (%d,%d)=%d\n", 
//        col,row,row+(col*bmp->width),byte,bit,x,y,byte+(y*bmp->pitch));
             pixels[(row * bmp->width) + col] = 0;
          } else {
             pixels[(row * bmp->width) + col] = 1;
          }
      }
    }      
    // Minimal font and per-glyph information is stored to
    // reduce flash space requirements.  Glyph bitmaps are
    // fully bit-packed; no per-scanline pad, though end of
    // each character may be padded to next byte boundary
    // when needed.  16-bit offset means 64K max for bitmaps,
    // code currently doesn't check for overflow.  (Doesn't
    // check that size & offsets are within bounds either for
    // that matter...please convert fonts responsibly.)

    table[j].c = i;
    table[j].x0 = pen_x;
    table[j].y0 = pen_y;
    table[j].x1 = pen_x + bmp->width;
    table[j].y1 = pen_y + bmp->rows;
    table[j].width = bmp->width;
    table[j].rows = bmp->rows;
    table[j].xOff = face->glyph->bitmap_left;
    table[j].yOff = face->glyph->bitmap_top;
//  table[j].yOff = 1 - face->glyph->bitmap_top;
    table[j].xAdvance = face->glyph->advance.x >> 6;
    table[j].pix = pixels;
    table[j].bitmapOffset = bitmapOffset;
    pen_x += bmp->width + 1;
      
    if (maxcharwidth < bmp->width) maxcharwidth = bmp->width;
    if (maxcharheight < bmp->rows) maxcharheight = bmp->rows;
    if ((maxgtop > table[j].yOff) && (i > 48)) maxgtop = table[j].yOff;

    bitmapOffset += (table[j].width * table[j].rows + 7) / 8;
    table[j].bitmapOffset = bitmapOffset;
    
    printf("// %d %c %5ld <%ld,%ld> ", j, i, memsize, 
            sizeof(bmp), sizeof(pixels));
    printTableInfo(stdout, table[j]); 
    printf(" (%3dx%3d)\n", maxcharwidth, maxcharheight);
//    FT_Done_Glyph(glyph);
  }
  FT_Done_FreeType(library);
  
  /// write png
  // char * png_data=(char *)calloc(tex_width*tex_height * 4, 1);
  // for (int i = 0, i < (tex_width * tex_height); i++) {
  //   png_data[i * 4 + 0] = pixels[i];
  //   png_data[i * 4 + 0] = pixels[i];
  //   png_data[i * 4 + 0] = pixels[i];
  //   png_data[i * 4 + 0] = pixels[i];
  // }
  // stbi_write_png("fontoutput.png", tex_width, tex_height, 4, png_data, tex_width * 4);
  // free png_data;
  
// ------------------------------------------------------------------------ 
// output font file in txt format, complete font file

  bitmapOffset = 0;
  sprintf(fileName, "%s.txt", fontName); 
  if ((outFile = fopen(fileName, "w")) == NULL) {
     printf("\nCannot open output file %s\n", fileName);
     return errno;
  }
  printf("Output file %s openend\n",fileName); 
  
  fprintf(outFile,".NAME %s\n", fontName);
  fprintf(outFile,".FONT_HEIGHT %d\n", maxcharheight);

  // Process glyphs and output huge bitmap data array
  for (i = first, j = 0; i <= last; i++, j++) {

    fprintf(outFile,".CHAR %d\n", j);
    fprintf(outFile,".NOTE %c 'char ",i);
    printTableInfo(outFile, table[j]);
    fprintf(outFile," %d\n", maxcharheight - maxgtop);
    fprintf(outFile,".WIDTH %d\n", table[j].width);
    fprintf(outFile,".HEIGHT %d\n", table[j].rows);

    charheight = 0;
    
    for (y = 0; y < table[j].rows; y++) {
      for (x = 0; x < table[j].width; x++) {
//        byte = x / 8;
//        bit = 0x80 >> (x & 7);
//        enbit_star(bitmap->buffer[y * bitmap->pitch + byte] & bit);
//        enbit_star(table[j].pix[y * table[j].width + x]);
        if (table[j].pix[y * table[j].width + x] == 0) {
           fprintf(outFile,".");
        } else {
           fprintf(outFile,"*");
        }
      }
      charheight++;
      fprintf(outFile,"\n");
//      if (maxcharwidth < table[j].width) maxcharwidth = table[j].width;
    }
//    if (maxcharwidth < charheight) maxcharwidth = charheight;
    bitmapOffset += (table[j].width * table[j].rows + 7) / 8;
    table[j].bitmapOffset = bitmapOffset;
    
    fprintf(outFile,".END // %d %c ", charheight,i);
    printTableInfo(outFile, table[j]);
    fprintf(outFile," # %2dx%2d %d %ld\n",
            maxcharwidth, maxcharheight, maxgtop, memsize); 
  }
  fprintf(outFile," };\n\n\n"); // End bitmap array
  fclose(outFile);
  
// ------------------------------------------------------------------------ 
// Output font in txt format, in layers of 8 bits
  
  for (fontrun = 1; ((fontrun * 8) - 8) < maxcharheight; fontrun++) {
  sprintf(fileName,"%s%dth.txt", fontName, fontrun);
  if ((outFile = fopen(fileName, "w")) == NULL) {
     printf("\nCannot open output file %s\n", fileName);
     return errno;
  }
  printf("Output file %s opened\n",fileName);
  
  bitmapOffset = 0;
  fprintf(outFile,".NAME %s\n", fontName);
  fprintf(outFile,".FONT_HEIGHT %d\n", maxcharheight);

  for (i = first, j = 0; i <= last; i++, j++) {
    
    fprintf(outFile,".CHAR %d\n", j);
    fprintf(outFile,".NOTE %c 'char ",i);
    printTableInfo(outFile, table[j]);
    fprintf(outFile," %d\n", maxcharheight - maxgtop);
    fprintf(outFile,".WIDTH %d\n", table[j].width);
    fprintf(outFile,".HEIGHT %d\n", table[j].rows);

    charheight = 0;
    
    for (y = 0; y < table[j].width; y++) {
      charheight = 0;
//      for (x=0; x <  (maxgtop - table[j].yOff); x++) {
      for (x = 2; 
           x < (maxcharheight + (maxgtop - table[j].yOff)); 
           x++) {
        if (((charheight >= (fontrun - 1) * 8) && 
             (charheight < (fontrun * 8)))) {
           fprintf(outFile,"-");
        }
        charheight++;
      }  
      for (x=0; x < table[j].rows; x++) {
//        byte= y / 8;
//        bit = 0x80 >> (y & 7);
        if (((charheight >= (fontrun - 1) * 8) && 
             (charheight < (fontrun * 8)))) {
           if (table[j].pix[(x * table[j].width) + y] == 0) {
              fprintf(outFile,".");
           } else {
              fprintf(outFile,"@");
           }
        } 
        charheight++;
      }
      for (x=charheight; 
//           x < (table[j].yOff - table[j].rows - maxgtop + 1);
           x < maxcharheight; 
           x++) {
        if (((charheight >= (fontrun - 1) * 8) && 
             (charheight < (fontrun * 8)))) {
           fprintf(outFile,"+");
        }
        charheight++;
      }
//      if (charheight != table[j].rows) 
//         printf("***** %d %c %d-%d\n", j, i, charheight, table[j].rows);
      // Pad end of char bitmap to next byte boundary if needed
      x = charheight & 7;
      if (x) {     // Pixel count not an even multiple of 8?
        x = 8 - x; // # bits to next multiple
        while (x--) {
           if (((charheight >= (fontrun - 1) * 8) && 
                (charheight < (fontrun * 8)) )) {
             fprintf(outFile,":");
           }
           charheight++;
        }
      }
      fprintf(outFile, "\n");
    }
    fprintf(outFile,".END // %d %c ", charheight,i);
    printTableInfo(outFile, table[j]);
    fprintf(outFile," # %2dx%2d %d %ld\n",
            maxcharwidth, maxcharheight, maxgtop, memsize); 
  }
  fprintf(outFile," };\n\n\n"); // End bitmap array
  fclose(outFile);
  }
  
// ------------------------------------------------------------------------ 
// Output font structure for ADAFruit_GFX format 
// complete font in one file

  strcpy(fileName, fontName);
  strcat(fileName, ".c");
  if ((outFile = fopen(fileName, "w")) == NULL) {
     printf("\nCannot open output file %s\n", fileName);
     return errno;
  }
  printf("Output file %s openend\n",fileName);
  
  fprintf(outFile,"// Font definition in AdaFruit GFX format\n");
  fprintf(outFile,"const GFXglyph %s PROGMEM = {\n", fontName);
  fprintf(outFile,"  (uint8_t  *)%sBitmaps,\n", fontName);
  fprintf(outFile,"  (GFXglyph *)%sGlyphs,\n", fontName);
  fprintf(outFile,"  0x%02X, 0x%02X, %d };\n\n", first, last, maxcharheight);
  fprintf(outFile,"// Char bbox [%d %d] Approx. %d+%d bytes, memsize %5ld\n", 
     maxcharheight, maxcharwidth,bitmapOffset,
     bitmapOffset + (last - first + 1) * 7 + 7, memsize);
  // Size estimate is based on AVR struct and pointer sizes;
  // actual size may vary.
  fprintf(outFile,"// offset, width, height, adv, xoff, yoff\n"); 
  // Output glyph attributes table (one per character)
  fprintf(outFile,"const GFXglyph %sGlyphs[] PROGMEM = {\n", fontName);
  for (i = first, j = 0; i <= last; i++, j++) {
    fprintf(outFile,"  { %5d, %3d, %3d, %3d, %4d, %4d }", 
           table[j].bitmapOffset, table[j].width, table[j].rows, 
           table[j].xAdvance, table[j].xOff, table[j].yOff);
    if (i < last) {
      fprintf(outFile,",   // 0x%02X", j);
      if ((i >= ' ') && (i <= '~')) {
        fprintf(outFile," '%c'", i);
      }
      fprintf(outFile,"\n");
    }
  }
  fprintf(outFile," }; // 0x%02X", last);
  if ((last >= ' ') && (last <= '~')) fprintf(outFile," '%c'", last);
  fprintf(outFile,"\n\n");
  
  fprintf(outFile,"const uint8_t %sBitmaps[] PROGMEM = {\n", fontName);
  // process glyps and output huge bitmap array in AdaFruit_GFX format
  for (i = first, j = 0; i <= last; i++, j++) {
    for (y = 0; y < table[j].rows; y++) {
      for (x = 0; x < table[j].width; x++) {
//        byte = x / 8;
//       bit = 0x80 >> (x & 7);
//        enbit(bitmap->buffer[y * bitmap->pitch + byte] & bit);
          encbit(outFile, table[j].pix[y * table[j].width + x]);
      }
    }
    // Pad end of char bitmap to next byte boundary if needed
    int n = (table[j].width * table[j].rows) & 7;
    if (n) {     // Pixel count not an even multiple of 8?
      n = 8 - n; // # bits to next multiple
      while (n--)
        encbit(outFile, 0);
    }
    fprintf(outFile,"\n");
  }
  fprintf(outFile," };\n\n"); // End bitmap draw array for AdaFruit_GFX
  fclose(outFile);
  
// ------------------------------------------------------------------------
// Output font structure for MAX7219 format.
// Complete font in one file
 
  strcpy(fileName, fontName);
  strcat(fileName, ".h");
  if ((outFile = fopen(fileName, "w")) == NULL) {
     printf("\nCannot open output file %s\n", fileName);
     return errno;
  }
  printf("Output file %s opened\n",fileName);
  
  fprintf(outFile,"const MD_MAX72XX::fontType_t %s[] PROGMEM = {\n", fontName);
  fprintf(outFile,"  F, 1, 0x%02X, 0x%02X, %d, \n\n", first, last, maxcharheight);
  for (i = first, j = 0; i <= last; i++, j++) {
  
    fprintf(outFile,"%d,", table[j].width);
    
    charheight = 0;

    for (y=0; y < table[j].width; y++) {
      charheight = 0;
      for (x = 2; 
           x < (maxcharheight + (maxgtop - table[j].yOff)); 
           x++) {
        encbit(outFile,0);
        charheight++;
      }  
      for (x=0; x < table[j].rows; x++) {
//        byte= y / 8;
//        bit = 0x80 >> (y & 7);
//        enbit(bitmap->buffer[y * bitmap->pitch + byte] & bit);
        if (table[j].pix[x * table[j].width + y] == 0) {
          encbit(outFile, 0);
        } else {
          encbit(outFile, 1);
        } 
        encbit(outFile, table[j].pix[x * table[j].width + y]);
        charheight++;
      }
      for (x=charheight; 
//           x < (table[j].yOff - table[j].rows - maxgtop);
           x < maxcharheight; 
           x++) {
        encbit(outFile,0);
        charheight++;
      }
      // Pad end of char bitmap to next byte boundary if needed
      x = charheight & 7;
      if (x) {     // Pixel count not an even multiple of 8?
        x = 8 - x; // # bits to next multiple
        while (x--) {
          encbit(outFile,0);
          charheight++;
        }
      }
    }
    fprintf(outFile,"\n// %d %c ", charheight, i);
    printTableInfo(outFile, table[j]);
    fprintf(outFile," # %2dx%2d %d %ld\n",
            maxcharwidth, maxcharheight, maxgtop, memsize); 
  }
  fprintf(outFile," };\n\n"); // End MAX7219 bitmap array
  fclose(outFile);
  
// ------------------------------------------------------------------------ 
// Output font structure for MAX7219 format, in layers of 8 bits

  if (maxcharheight > 8) { 
  for (fontrun = 1; ((fontrun * 8) - 8) < maxcharheight; fontrun++) {
  sprintf(fileName,"%s%dth.h", fontName, fontrun);
  if ((outFile = fopen(fileName, "w")) == NULL) {
     printf("\nCannot open output file %s\n", fileName);
     return errno;
  }
//  printf("Output file %s opened\n",fileName);
   
  fprintf(outFile,"const MD_MAX72XX::fontType_t %s%dth[] PROGMEM = {\n", fontName, fontrun);
  fprintf(outFile,"  F, 1, 0x%02X, 0x%02X, %d, \n\n", first, last, 8);
  
  glyphwritten = 0;
 
  for (i = first, j = 0; i <= last; i++, j++) {
  
    fprintf(outFile,"%d,", table[j].width);
    
    charheight = 0;
    bitwritten = 0;
    
    for (y = 0; y < table[j].width; y++) {
    
      charheight = 0;
      bitwritten = 0;
      for (x = 2; 
           x < (maxcharheight + (maxgtop - table[j].yOff)); 
           x++) {
        if (((charheight >= (fontrun - 1) * 8) && 
             (charheight < (fontrun * 8)))) {
           encbit(outFile,0);
        }
        charheight++;
      }  
      for (x=0; x < table[j].rows; x++) {
 //       byte= y / 8;
 //       bit = 0x80 >> (y & 7);
        if (((charheight >= (fontrun - 1) * 8) && 
             (charheight < (fontrun * 8)))) {
           if (table[j].pix[x * table[j].width + y] == 0) {
              encbit(outFile, 0);
           } else {
              encbit(outFile, 1);
              bitwritten += 1;
           } 
        }
        charheight++;
      }
      for (x=charheight; 
//         x < (table[j].yOff - table[j].rows - maxgtop); 
           x < maxcharheight;
           x++) {
        if (((charheight >= (fontrun - 1) * 8) && 
             (charheight < (fontrun * 8)))) {
           encbit(outFile,0);
        }
        charheight++;
      }
      // Pad end of char bitmap to next byte boundary if needed
      x = charheight & 7;
      if (x) {     // Pixel count not an even multiple of 8?
        x = 8 - x; // # bits to next multiple
        while (x--) {
          if (((charheight >= (fontrun - 1) * 8) && 
              (charheight < (fontrun * 8)))) {
            encbit(outFile,0);
          }
          charheight++;
        }
      }
    }
    if (bitwritten > 0) glyphwritten += 1;
    if ((bitwritten > 0) && (((fontrun * 8)) > maxcharheight)) {
        printf("// %d %c %5ld [%d %d] ", j, i, memsize, fontrun, maxcharheight);
        printTableInfo(stdout, table[j]); 
        printf(" (%3dx%3d)\n", maxcharwidth, maxcharheight);
    }
    
    fprintf(outFile,"\n// %d %c ", charheight, i);
    printTableInfo(outFile, table[j]);
    fprintf(outFile," # %2dx%2d %d %ld\n",
            maxcharwidth, maxcharheight, maxgtop, memsize); 
  }
  fprintf(outFile," };\n\n"); // End MAX7219 bitmap array
  fclose(outFile);
  printf("Font file %s contains %d non-zero glyphs\n",fileName,glyphwritten);
  if (glyphwritten == 0) {
    printf("Font file %s is completely filled with zeroes\n",fileName);
  }
  }
  }
// free (png_data);
//  free(table);
    
  return 0;
}

/* -------------------------------------------------------------------------

Character metrics are slightly different from classic GFX & ftGFX.
In classic GFX: cursor position is the upper-left pixel of each 5x7
character; lower extent of most glyphs (except those w/descenders)
is +6 pixels in Y direction.
W/new GFX fonts: cursor position is on baseline, where baseline is
'inclusive' (containing the bottom-most row of pixels in most symbols,
except those with descenders; ftGFX is one pixel lower).

Cursor Y will be moved automatically when switching between classic
and new fonts.  If you switch fonts, any print() calls will continue
along the same baseline.

                    ...........#####.. -- yOffset
                    ..........######..
                    ..........######..
                    .........#######..
                    ........#########.
   * = Cursor pos.  ........#########.
                    .......##########.
                    ......#####..####.
                    ......#####..####.
       *.#..        .....#####...####.
       .#.#.        ....##############
       #...#        ...###############
       #...#        ...###############
       #####        ..#####......#####
       #...#        .#####.......#####
====== #...# ====== #*###.........#### ======= Baseline
                    || xOffset

glyph->xxOffset and yOffset are pixel offsets, in GFX coordinate space
(+Y is down), from the cursor position to the top-left pixel of the
glyph bitmap.  i.e. yOffset is typically negative, xOffset is typically
zero but a few glyphs will have other values (even negative xOffsets
sometimes, totally normal).  glyph->xAdvance is the distance to move
the cursor on the X axis after drawing the corresponding symbol.

There's also some changes with regard to 'background' color and new GFX
fonts (classic fonts unchanged).  See Adafruit_GFX.cpp for explanation.
*/

#endif /* !ARDUINO */
