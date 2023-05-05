
#define PROGNAME "PNG-TILESET"
#define VERSION "1.0 of 4 May 2023"
#define APPNAME "Simple PNG to Tileset & CSV Converter"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <png.h>
#include <zlib.h>

#define MAX_FILENAME 256
#define tpng_error(x) {fprintf(stderr,PROGNAME ": " x);}
#define TPNG_BPP (tpng.bit_depth >> 3)
#define rnm(x,y) ((x/y)*y)
#define ceil(x,y) ((unsigned int)((float)x/(float)y+0.5f))

typedef unsigned char uch;
/* struct for parameters and infile properties*/
typedef struct tpng_struct {
	int convert_binary;
	unsigned int width_screen_px;
	unsigned int height_screen_px;
	unsigned int width_tileset_intiles;
	unsigned int tile_size;

	unsigned int width_px;
	unsigned int height_px;
	int bit_depth;
	int color_type;
	unsigned int width_row_bytes;
	unsigned int image_channels; 
} Tpng;

FILE *infile,*outfile_png,*outfile_csv;
png_structp png_ptr;
png_infop info_ptr;
int *csv;
uch *tileset_data;
uch *image_data;
void tpng_free(Tpng*);

int main(int argc,char **argv) {
	if (argc < 2
	||	!strcmp(argv[1],"--help")
	||	!strcmp(argv[1],"-h")) {
		fprintf(stderr,"\nUsage: %s [infile] [options]\n",PROGNAME);
		fprintf(stderr,"Options:\n");
		fprintf(stderr,"\t-b\t\t\tReplace CSV with binary file\n");
		fprintf(stderr,"\t-ws[N}\t\t\tSpecify width of screen (px)\n");
		fprintf(stderr,"\t-hs[N}\t\t\tSpecify height of screen (px)\n");
		fprintf(stderr,"\t-w[N}\t\t\tSpecify width of tileset (in tiles)\n");
		fprintf(stderr,"\t-t[N]\t\t\tSpecify tile size (px)\n");
		fprintf(stderr,"\t-h,--help\t\tView options\n");
		return 0;
	}

	Tpng tpng;
	tpng.convert_binary = 0;
	tpng.tile_size = 16;
	tpng.width_tileset_intiles = 16;
	tpng.width_screen_px = 320;
	tpng.height_screen_px = 192;
	for (int i=2;i<argc;i++) {
		if (!strcmp(argv[i],"-b")) {
			tpng.convert_binary = 1;
		}
		if (!strncmp(argv[i],"-w",2)) {
			argv[i] += 2;
			tpng.width_tileset_intiles = atoi(argv[i]);
			if (tpng.width_tileset_intiles == 0) {
				tpng_error("Trailing characters");
				return -1;
			}
		}
		if (!strncmp(argv[i],"-t",2)) {
			argv[i] += 2;
			tpng.tile_size = atoi(argv[i]);
			if (tpng.tile_size == 0) {
				tpng_error("Trailing characters");
				return -1;
			}
		}
		if (!strncmp(argv[i],"-ws",3)) {
			argv[i] += 3;
			tpng.width_screen_px = atoi(argv[i]);
			if (tpng.width_screen_px == 0) {
				tpng_error("Trailing characters");
				return -1;
			}
		}
		if (!strncmp(argv[i],"-hs",3)) {
			argv[i] += 3;
			tpng.height_screen_px = atoi(argv[i]);
			if (tpng.height_screen_px == 0) {
				tpng_error("Trailing characters");
				return -1;
			}
		}
		else {
			tpng_error("Trailing characters");
			return -1;
		}
	}
	infile = fopen(argv[1],"rb");
	if (infile == NULL) {
		tpng_error("Can't open input file");
		return -1;
	}


	char *inname = argv[1];
	char outname[MAX_FILENAME];
	int len = strlen(inname);
	if (len >= MAX_FILENAME) {
		tpng_error("Input file name too long");
		tpng_free(&tpng);
		return -1;
	}
	char *p = strrchr(inname,'.');
	if (p == NULL || p-inname != len-4) {
		strcpy(outname,inname);
		strcpy(outname+len,"_tileset.png");
	}
	else {
		len -= 4;
		strncpy(outname,inname,len);
		strcpy(outname+len,"_tileset.png");
	}
	outfile_png = fopen(outname,"wb");
	if (outfile_png == NULL) {
		tpng_error("can't open output file");
		tpng_free(&tpng);
		return -1;
	}


	if (tpng.convert_binary) {
		strcpy(outname+len,".bin");
		outfile_csv = fopen(outname,"wb");
		if (outfile_csv == NULL) {
			tpng_error("can't open output file");
			tpng_free(&tpng);
			return -1;
		}
	}
	else {
		strcpy(outname+len,".csv");
		outfile_csv = fopen(outname,"w");
		if (outfile_csv == NULL) {
			tpng_error("can't open output file");
			tpng_free(&tpng);
			return -1;
		}
	}

	uch sig[8];
	fread(sig,1,8,infile);
	if (!png_check_sig(sig,8)) {
		tpng_error("missing png signature");
		tpng_free(&tpng);
		return -1;
	}
	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,NULL,NULL,NULL);
	if (png_ptr == NULL) {
		tpng_error("png_struct init failure");
		tpng_free(&tpng);
		return -1;
	}
	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL) {
		png_destroy_read_struct(&png_ptr,NULL,NULL);
		png_ptr = NULL;
		tpng_error("png_info init failure");
		tpng_free(&tpng);
		return -1;
	}
	if (setjmp(png_jmpbuf(png_ptr))) {
		tpng_error("libpng longjmp failure");
		tpng_free(&tpng);
		return -1;
	}

	png_init_io(png_ptr,infile);
	png_set_sig_bytes(png_ptr,8);
	png_read_info(png_ptr,info_ptr);
	png_get_IHDR(
		png_ptr,info_ptr,&tpng.width_px,&tpng.height_px,
		&tpng.bit_depth,&tpng.color_type,NULL,NULL,NULL
	);
	if (tpng.width_px % tpng.tile_size
	||	tpng.height_px % tpng.tile_size) {
		tpng_error("Image dimensions have to be multiple of tile size");
		tpng_free(&tpng);
		return -1;
	}
	if (tpng.width_px % tpng.width_screen_px
	||	tpng.height_px % tpng.height_screen_px) {
		tpng_error("Image dimensions have to be multiple of screen size");
		tpng_free(&tpng);
		return -1;
	}
	if (tpng.width_screen_px % tpng.tile_size
	||	tpng.height_screen_px % tpng.tile_size) {
		tpng_error("Screen size has to be multiple of tile size");
		tpng_free(&tpng);
		return -1;
	}
	if (tpng.tile_size >= tpng.width_px
	||	tpng.tile_size >= tpng.height_px) {
		tpng_error("Tile size cannot exceed image dimensions");
		tpng_free(&tpng);
		return -1;
	}	
	if (tpng.width_px < tpng.width_screen_px
	||	tpng.height_px < tpng.height_screen_px) {
		tpng_error("Screen size cannot exceed image dimensions");
		tpng_free(&tpng);
		return -1;
	}
	if (tpng.tile_size >= tpng.width_screen_px
	||	tpng.tile_size >= tpng.height_screen_px) {
		tpng_error("Tile size cannot exceed screen size");
		tpng_free(&tpng);
		return -1;
	}	

	int expandCond,rgbCond;
	expandCond  = tpng.color_type == PNG_COLOR_TYPE_PALETTE && tpng.bit_depth < 8;
	expandCond |= tpng.color_type == PNG_COLOR_TYPE_GRAY && tpng.bit_depth < 8;
	expandCond |= png_get_valid(png_ptr,info_ptr,PNG_INFO_tRNS);
	if (expandCond) {png_set_expand(png_ptr);}
	if (tpng.bit_depth == 16) {png_set_scale_16(png_ptr);}
	rgbCond = tpng.color_type == PNG_COLOR_TYPE_GRAY;
	rgbCond |= tpng.color_type == PNG_COLOR_TYPE_GRAY_ALPHA;
	if (rgbCond) {png_set_gray_to_rgb(png_ptr);}
	png_read_update_info(png_ptr,info_ptr);


	tpng.width_row_bytes = png_get_rowbytes(png_ptr,info_ptr);
	tpng.image_channels = png_get_channels(png_ptr,info_ptr);
	image_data = malloc(tpng.width_row_bytes * tpng.height_px);
	if (image_data == NULL) {
		tpng_error("insufficient memory");
		tpng_free(&tpng);
		return -1;
	}
	png_bytep row_pointers[tpng.height_px];
	for (int i=0;i<tpng.height_px;i++) {
		row_pointers[i] = image_data + i*tpng.width_row_bytes;
	}
	png_read_image(png_ptr,row_pointers);
	png_read_end(png_ptr,NULL);
	if (image_data == NULL) {
		tpng_error("unable to decode PNG image");
		tpng_free(&tpng);
		return -1;
	}
	fclose(infile);
	png_destroy_read_struct(&png_ptr,&info_ptr,NULL);
	png_ptr = NULL;
	info_ptr = NULL;

	tileset_data = malloc(tpng.width_row_bytes * tpng.height_px);
	if (tileset_data == NULL) {
		tpng_error("insufficient memory");
		tpng_free(&tpng);
		return -1;
	}
	unsigned int width_intiles,height_intiles;
	width_intiles = tpng.width_px / tpng.tile_size;
	height_intiles = tpng.height_px / tpng.tile_size;
	csv = malloc(sizeof(int) * width_intiles * height_intiles);
	if (csv == NULL) {
		tpng_error("insufficient memory");
		tpng_free(&tpng);
		return -1;
	}

	unsigned int width_screen_intiles,height_screen_intiles;
	width_screen_intiles = tpng.width_screen_px / tpng.tile_size;
	height_screen_intiles = tpng.height_screen_px / tpng.tile_size;
	unsigned int width_tileset_px,height_tileset_px;
	width_tileset_px = tpng.width_tileset_intiles * tpng.tile_size;
	unsigned int width_tileset_bytes;
	width_tileset_bytes = tpng.width_row_bytes * ((float)width_tileset_px/(float)tpng.width_px);
	unsigned int tileset_len = 0;

	for (int row=0;row<height_intiles;row++) {
		int csv_index_a = 0;
		csv_index_a = rnm(row,height_screen_intiles)*width_intiles;
		csv_index_a += (row-rnm(row,height_screen_intiles))*width_screen_intiles;

		for (int col=0;col<width_intiles;col++) {
			int csv_index_b = 0;
			csv_index_b = rnm(col,width_screen_intiles)*height_screen_intiles;
			csv_index_b += (col-rnm(col,width_screen_intiles));

			uch *tile1,*tile2;
			int off = 0;
			off = row*tpng.width_row_bytes
				+ col*tpng.image_channels*TPNG_BPP;
			off *= tpng.tile_size;
			tile1 = image_data + off;
			int amt_rows_same = 0;
			for (int k=0;k<tileset_len;k++) {
				amt_rows_same = 0;
				off = k/tpng.width_tileset_intiles*width_tileset_bytes
					+ (k-rnm(k,tpng.width_tileset_intiles))*tpng.image_channels*TPNG_BPP;
				off *= tpng.tile_size;
				tile2 = tileset_data + off;
				for (int l=0;l<tpng.tile_size;l++) {
					if (!memcmp(
						tile1+l*tpng.width_row_bytes,
						tile2+l*width_tileset_bytes,
						tpng.image_channels*TPNG_BPP*tpng.tile_size)) {amt_rows_same++;}
				}
				if (amt_rows_same == tpng.tile_size) {
					memcpy(csv+csv_index_a+csv_index_b,&k,sizeof(int));
					break;
				}
			}
			if (amt_rows_same != tpng.tile_size) {
				int k = tileset_len;
				off = k/tpng.width_tileset_intiles*width_tileset_bytes
					+ (k-rnm(k,tpng.width_tileset_intiles))*tpng.image_channels*TPNG_BPP;
				off *= tpng.tile_size;
				tile2 = tileset_data + off;
				for (int l=0;l<tpng.tile_size;l++) {
					memcpy(
						tile2+l*width_tileset_bytes,
						tile1+l*tpng.width_row_bytes,
						tpng.image_channels*tpng.tile_size);
				}
				memcpy(csv+csv_index_a+csv_index_b,&tileset_len,sizeof(int));
				tileset_len++;
			}
		}
	}

	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,NULL,NULL,NULL);
	if (png_ptr == NULL) {
		tpng_error("insufficient memory");
		tpng_free(&tpng);
		return -1;
	}
	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL) {
		png_destroy_write_struct(&png_ptr,NULL);
		png_ptr = NULL;
		tpng_error("insufficient memory");
		tpng_free(&tpng);
		return -1;
	}
	if (setjmp(png_jmpbuf(png_ptr))) {
		tpng_error("libpng longjmp failure");
		tpng_free(&tpng);
		return -1;
	}


	height_tileset_px = ceil(tileset_len,tpng.width_tileset_intiles) * tpng.tile_size;
	png_init_io(png_ptr,outfile_png);
	png_set_compression_level(png_ptr,Z_BEST_COMPRESSION);
	png_set_IHDR(
		png_ptr,info_ptr,width_tileset_px,height_tileset_px,
		tpng.bit_depth,tpng.color_type,PNG_INTERLACE_NONE,
		PNG_COMPRESSION_TYPE_DEFAULT,PNG_FILTER_TYPE_DEFAULT
	);
	png_write_info(png_ptr,info_ptr);
	png_set_packing(png_ptr);

	png_bytep tileset_pointers[height_tileset_px];
	for (int i=0;i<height_tileset_px;i++) {
		tileset_pointers[i] = tileset_data + i*width_tileset_bytes;
	}
	png_write_image(png_ptr,tileset_pointers);
	png_write_end(png_ptr,NULL);
	png_destroy_write_struct(&png_ptr,&info_ptr);
	png_ptr = NULL;
	info_ptr = NULL;
	fclose(outfile_png);


	if (!tpng.convert_binary) {
		for (int i=0;i<width_intiles*height_intiles;i++) {
			/*
			if (!(i % (width_screen_intiles*6))) {
				fprintf(outfile_csv,"\n");
			}
			*/
			if (!(i % width_screen_intiles)) {
				fprintf(outfile_csv,"\n");
			}
			if (!(i % (width_screen_intiles*height_screen_intiles))) {
				fprintf(outfile_csv,"\n");
			}
			/*
			if (!(i % 5)) {
				fprintf(outfile_csv," ");
			}
			*/
			fprintf(outfile_csv,"%d,",csv[i]);	
		}
	}
	else {fputs((const char*)csv,outfile_csv);}
	fclose(outfile_csv);

	tpng_free(&tpng);
	return 0;
}

void tpng_free(Tpng *tpng) {
	if (png_ptr != NULL) {png_destroy_read_struct(&png_ptr,&info_ptr,NULL);}
	if (csv != NULL) {free(csv);}
	if (image_data != NULL) {free(image_data);}
	if (tileset_data != NULL) {free(tileset_data);}
	if (infile != NULL) {fclose(infile);}
	if (outfile_png != NULL) {fclose(outfile_png);}
	if (outfile_csv != NULL) {fclose(outfile_csv);}
}
