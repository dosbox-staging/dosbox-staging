#define STB_DEFINE
#define STB_ONLY
#include "stb.h"           /*  http://nothings.org/stb.h  */
#include "stb_vorbis.h"

void write_floats(FILE *g, int len, float *left, float *right);
void show_info(stb_vorbis *v);

// stb_vorbis_decode_filename: decode an entire file to interleaved shorts
void test_decode_filename(FILE *g, char *filename)
{
   short *decoded;
   int channels, len, sample_rate;
   len = stb_vorbis_decode_filename(filename, &channels, &sample_rate, &decoded);

   if (len)
      fwrite(decoded, 2, len*channels, g);
   else
      stb_fatal("Couldn't open {%s}", filename);
}

void test_get_frame_short_interleaved(FILE *g, char *filename)
{
   short sbuffer[8000];
   int n, error;
   stb_vorbis *v = stb_vorbis_open_filename(filename, &error, NULL);
   if (!v) stb_fatal("Couldn't open {%s} due to error: %d", filename, error);
   show_info(v);

   while (0 != (n=stb_vorbis_get_frame_short_interleaved(v, 2, sbuffer, 8000))) {
      fwrite(sbuffer, 2, n*2, g);
   }
   stb_vorbis_close(v);
}

void test_get_samples_short_interleaved(FILE *g, char *filename)
{
   int error;
   stb_vorbis *v = stb_vorbis_open_filename(filename, &error, NULL);
   if (!v) stb_fatal("Couldn't open {%s}", filename);
   show_info(v);

   for(;;) {
      int16 sbuffer[333];
      int n;
      n = stb_vorbis_get_samples_short_interleaved(v, 2, sbuffer, 333);
      if (n == 0)
         break;
      fwrite(sbuffer, 2, n*2, g);
   }
   stb_vorbis_close(v);
}

void test_get_frame_float(FILE *g, char *filename)
{
   int error;
   stb_vorbis *v = stb_vorbis_open_filename(filename, &error, NULL);
   if (!v) stb_fatal("Couldn't open {%s}", filename);
   show_info(v);

   for(;;) {
      int n;
      float *left, *right;
      float **outputs;
      int num_c;
      n = stb_vorbis_get_frame_float(v, &num_c, &outputs);
      if (n == 0)
         break;
      left = outputs[0];
      right = outputs[num_c > 1];
      write_floats(g, n, left, right);
   }
   stb_vorbis_close(v);
}


// in push mode, you can load your data any way you want, then
// feed it a little bit at a time. this is the preferred way to
// handle reading from a packed file or some custom stream format;
// instead of putting callbacks inside stb_vorbis, you just keep
// a little buffer (it needs to be big enough for one packet of
// audio, except at the beginning where you need to buffer up the
// entire header).
//
// for this test, I just load all the data and just lie to stb_vorbis
// and claim I only have a little of it
void test_decode_frame_pushdata(FILE *g, char *filename)
{
   int p,q, len, error, used;
   stb_vorbis *v;
   uint8 *data = stb_file(filename, &len);

   if (!data) stb_fatal("Couldn't open {%s}", filename);

   p = 0;
   q = 1;
  retry:
   v = stb_vorbis_open_pushdata(data, q, &used, &error, NULL);
   if (v == NULL) {
      if (error == VORBIS_need_more_data) {
         q += 1;
         goto retry;
      }
      fprintf(stderr, "Error %d\n", error);
      exit(1);
   }
   p += used;

   show_info(v);

   for(;;) {
      int k=0;
      int n;
      float *left, *right;
      uint32 next_t=0;

      float **outputs;
      int num_c;
      q = 32;
     retry3:
      if (q > len-p) q = len-p;
      used = stb_vorbis_decode_frame_pushdata(v, data+p, q, &num_c, &outputs, &n);
      if (used == 0) {
         if (p+q == len) break; // no more data, stop
         if (q < 128) q = 128;
         q *= 2;
         goto retry3;
      }
      p += used;
      if (n == 0) continue; // seek/error recovery
      left = outputs[0];
      right = num_c > 1 ? outputs[1] : outputs[0];
      write_floats(g, n, left, right);
   }
   stb_vorbis_close(v);
}

void write_floats(FILE *g, int len, float *left, float *right)
{
   const float scale = 32768;
   int j;
   for (j=0; j < len; ++j) {
      int16 x,y;
      x = (int) stb_clamp((int) (scale * left[j]), -32768, 32767);
      y = (int) stb_clamp((int) (scale * right[j]), -32768, 32767);
      fwrite(&x, 2, 1, g);
      fwrite(&y, 2, 1, g);
   }
}

void show_info(stb_vorbis *v)
{
   if (v) {
      stb_vorbis_info info = stb_vorbis_get_info(v);
      printf("%d channels, %d samples/sec\n", info.channels, info.sample_rate);
      printf("Predicted memory needed: %d (%d + %d)\n", info.setup_memory_required + info.temp_memory_required,
                info.setup_memory_required, info.temp_memory_required);
   }
}

int main(int argc, char **argv)
{
   FILE *g=NULL;
   char *outfile = "vorbis_test.out";
   char *infile;
   int n=0;

   if (argc > 1 && argv[1][1] == 0 && argv[1][0] >= '1' && argv[1][0] <= '5')
      n = atoi(argv[1]);

   if (argc < 3 || argc > 4 || n == 0) {
      stbprint("Usage: sample {code} {vorbis-filename} [{output-filename}]\n"
                           "Code is one of:\n"
                           "    1  -  test stb_vorbis_decode_filename\n"
                           "    2  -  test stb_vorbis_get_frame_short_interleaved\n"
                           "    3  -  test stb_vorbis_get_samples_short_interleaved\n"
                           "    4  -  test stb_vorbis_get_frame_float\n"
                           "    5  -  test stb_vorbis_decode_frame_pushdata\n");
      return argc != 1;
   }

   infile = argv[2];

   if (argc > 3)
      outfile = argv[3];
   if (strlen(outfile) >= 4 && 0==stb_stricmp(outfile + strlen(outfile) - 4, ".ogg"))
      stb_fatal("You specified a .ogg file as your output file, which you probably didn't actually want.");

   if (!strcmp(outfile, "stdout") || !strcmp(outfile, "-") || !strcmp(outfile, "-stdout"))
      g = stdout;
   else
      g = fopen(outfile, "wb");
   if (!g) stb_fatal("Couldn't open {%s} for writing", outfile);

   switch (n) {
      case 1: test_decode_filename(g, infile); break;
      case 2: test_get_frame_short_interleaved(g, infile); break;
      case 3: test_get_samples_short_interleaved(g, infile); break;
      case 4: test_get_frame_float(g, infile); break;
      case 5: test_decode_frame_pushdata(g, infile); break;
      default: stb_fatal("Unknown option {%d}", n);
   }
   fclose(g);
   return 0;
}

void test_push_mode_forever(FILE *g, char *filename)
{
   int p,q, len, error, used;
   uint8 *data = stb_file(filename, &len);
   stb_vorbis *v;

   if (!data) stb_fatal("Couldn't open {%s}", filename);

   p = 0;
   q = 1;
  retry:
   v = stb_vorbis_open_pushdata(data, q, &used, &error, NULL);
   if (v == NULL) {
      if (error == VORBIS_need_more_data) {
         q += 1;
         goto retry;
      }
      printf("Error %d\n", error);
      exit(1);
   }
   p += used;

   show_info(v);

   for(;;) {
      int k=0;
      int n;
      float *left, *right;
      float **outputs;
      int num_c;
      q = 32;
      retry3:
      if (q > len-p) q = len-p;
      used = stb_vorbis_decode_frame_pushdata(v, data+p, q, &num_c, &outputs, &n);
      if (used == 0) {
         if (p+q == len) {
            // seek randomly when at end... this makes sense when listening to it, but dumb when writing to file
            p = stb_rand();
            if (p < 0) p = -p;
            p %= (len - 8000);
            stb_vorbis_flush_pushdata(v);
            q = 128;
            goto retry3;
         }
         if (q < 128) q = 128;
         q *= 2;
         goto retry3;
      }
      p += used;
      if (n == 0) continue;
      left = outputs[0];
      right = num_c > 1 ? outputs[1] : outputs[0];
      write_floats(g, n, left, right);
   }
   stb_vorbis_close(v);
}
