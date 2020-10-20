/***************************************************************************************
 cnf2txt.c
 Convert Genie2k binary data files to ascii.

 Stephan Messlinger
 2016-10-10
*/

#include <string.h>
#include <time.h>
#include <stdio.h>
#include <malloc.h>
#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>


#if !(defined _WIN32 && defined _MSC_VER)
	#include <inttypes.h>
#endif

#if (defined _WIN32 && defined _MSC_VER)
	#define _CRT_SECURE_NO_WARNINGS  1
	#define _SCL_SECURE_NO_WARNINGS  1
	#pragma warning(disable:4996)
#endif

/***************************************************************************************/

void* malloc_exit( size_t size )
{
	char* buffer = malloc(size);
	if (buffer==NULL) {
		puts("Memory error.");
		exit(1);
	}
	return buffer;
}

uint8_t  uint8_at( const char* p )
{
	return *(uint8_t*)p;
}

uint16_t  uint16_at( const char* p )
{
	return *(uint16_t*)p;
}

uint32_t  uint32_at( const char* p )
{
	return *(uint32_t*)p;
}

uint64_t  uint64_at( const char* p )
{
	return *(uint64_t*)p;
}

double  time_at( const char* p )
{
	/* Period of time stored in units of 0.1 us. */
	return ~uint64_at(p) * 1e-7;
}

time_t  datetime_at( const char* p )
{
	/* Absolute date: Modified Julian date in 0.1 us resolution */
	return uint64_at(p) / 10000000UL - 3506716800UL;
}

double  pdp11f_at( const char* p )
{
	uint16_t  tmp16[2];
	tmp16[0] = ((uint16_t*)p)[1];
	tmp16[1] = ((uint16_t*)p)[0];
	return (*(float*)tmp16)/4.;
}

int verify( const char* p, size_t data_len )
{
	if (data_len<0x1000) return 0;

    /* Test for magic strings */
    return !strncmp( p+0x001e, "Associated", 10 );
}

int  fread_all( char** buffer, FILE* f )
{
	size_t  len;
	if (!f) return 0;
	if (*buffer) free(*buffer);
	fseek(f, 0, SEEK_END);
	len = ftell(f);
	*buffer = malloc_exit(len);
	rewind(f);
	len = fread( *buffer, 1, len, f );
	return len;
}

double energy( int ch, double* A )
{
	return A[0] + A[1]*ch + A[2]*ch*ch + A[3]*ch*ch*ch;
}

char* trim_str( char* str )
{
	char *stop=NULL;
	while (!isalpha(*str)) {
		if (*str==0) return str;
		str++;
	}
	stop = str;
	while (isalpha(*stop)) stop++;
	*stop = 0;
	return str;
}

int is_pow2( int i )
{
	return (i>0 && !(i & (i - 1)));
}

void usage_error( void )
{
	puts("Usage: cnf2txt input_file [output_file]");
	exit(1);
}

/***************************************************************************************/

int main( int argc, char* argv[] )
{
	const char *filename=NULL;
	char *ofname=NULL, *pext=NULL;
	char *data=NULL;
	size_t data_len=0;
	FILE *cnf=NULL, *txt=NULL;
	size_t offs_param=0, offs_str=0, offs_chan=0, offs_mark=0;
	size_t offs_calib=0, offs_times=0; 
	const char *sample_name=NULL, *sample_id=NULL, *sample_type=NULL,
		        *sample_unit=NULL, *user_name=NULL, *sample_desc=NULL,
		        *energy_unit=NULL;
	double A[4]={0}, real_time=0, live_time=0;
	time_t  start_time=0;
	char start_time_str[40]="";
	int n_channels=0;
	int i=0;
	int marker_left=0, marker_right=0;
	uint64_t total_counts=0, marker_counts=0;
	uint32_t *channels=NULL;
	int resample=0, merge=0;
	int argsk = 0;


	if (argc<2) usage_error();
	filename = argv[1+argsk];
	if( strchr(filename, '*') || strchr( filename, '?') ) {
		puts("Sorry, no wildcards supported.");
		exit(1);
	}

	if (argc > 2+argsk) {
		ofname = argv[2+argsk];
	} else {
		ofname = malloc_exit( 10+strlen(filename) );
		strcpy( ofname, filename );
		pext = strrchr(ofname, '.');
		if (!pext) pext = ofname+strlen(ofname);
		strcpy(pext,".txt");
	}

	cnf = fopen(filename, "rb");
	if (!cnf) {
		int err = errno;
		printf("Error opening file %s: %s\n", filename, strerror(err) );
		exit(1);
	}

	data_len = fread_all(&data, cnf);
	if (!data_len) {
		int err = errno;
		printf("Error reading from file %s: %s\n", filename, strerror(err) );
		exit(1);
	}

	if (!verify(data, data_len)) {
		printf("File %s: Format error.\n", filename );
		exit(1);
	}

	/* Parse header */
	for (i=0; ;i++)
	{
		size_t oh = 0x70 + i*0x30;       /* List of available section headers starts at 0x70 */
		uint32_t idh=0, offs=0;
		if (oh+0x20 > data_len) {
			printf("File %s: Format error.\n", filename );
			exit(1);
		}
		idh = uint32_at(data + oh);            /* Section id in header (identifies type of section) */
		if (idh==0x00000000)  break;           /* end of section list */

		offs = uint32_at(data + oh + 0x0a);    /* Start of section in data file */
		
		if      (idh==0x00012000)  offs_param = offs;  /* Known section ids */
		else if (idh==0x00012001)  offs_str = offs;
		else if (idh==0x00012005)  offs_chan = offs;
		else if (idh==0x00012004)  offs_mark = offs;
		else continue;

		if (idh != uint32_at(data + offs) ) {  /* For known sections: Section header is repeated in section block */
			printf("File %s: Format error.\n", filename );
			exit(1);
		}
	}
	if (offs_param==0 || offs_str==0 || offs_chan==0
		              || offs_mark==0) {
		printf("File %s: Format error.\n", filename );
		exit(1);
	}

	/* Strings */
	if (offs_str + 0x0470 > data_len ) {
		printf("File %s: Format error.\n", filename );
		exit(1);
	}
	sample_name = data + offs_str + 0x0030; /* max 0x40 */
	sample_id = data + offs_str + 0x0070;    /* max 0x40 */
	sample_type = data + offs_str + 0x00b0;  /* max 0x10 */
	sample_unit = data + offs_str + 0x00c4; /* max 0x40 */
	user_name = data + offs_str + 0x02d6;   /* max 0x18 */
	sample_desc = data + offs_str + 0x036e; /* max 0x100 */

	if (offs_param + 0x0452 > data_len ) {
		printf("File %s: Format error.\n", filename );
		exit(1);
	}
	/* Energy calibration coefficients */
	offs_calib = offs_param + 0x30 + uint16_at(data + offs_param + 0x22);
	A[0] = pdp11f_at(data + offs_calib + 0x44);
	A[1] = pdp11f_at(data + offs_calib + 0x48);
	A[2] = pdp11f_at(data + offs_calib + 0x4c);
	A[3] = pdp11f_at(data + offs_calib + 0x50);
	energy_unit = trim_str( data + offs_calib + 0x5c );

	/* Times */
	offs_times = offs_param + 0x30 + uint16_at(data + offs_param + 0x24);
	real_time = time_at( data + offs_times + 0x09 );
	live_time = time_at( data + offs_times + 0x11 );
	start_time = datetime_at( data + offs_times + 0x01 );
	strftime( start_time_str, sizeof(start_time_str), "%Y-%m-%d, %H:%M:%S", gmtime(&start_time) );

	/* Channel data */
	n_channels = uint8_at( data + offs_param + 0x00ba ) * 256;
	if (offs_chan + 0x200 + 4*n_channels > data_len
		  || !is_pow2(n_channels) ) {
		printf("File %s: Format error.\n", filename );
		exit(1);
	}
	channels = malloc_exit( n_channels*4 );
	total_counts = 0;
	for (i=0; i<n_channels; i++) {
		channels[i] = uint32_at( data + offs_chan+0x200 + 4*i );
		total_counts += channels[i];
	}

	/* Marker */
	marker_left  = uint32_at( data + offs_mark + 0x007a );
	marker_right = uint32_at( data + offs_mark + 0x008a );
	marker_counts = 0;
	for (i=marker_left; i<marker_right; i++) {
		marker_counts += channels[i];
	}

	/* Assemble output file */
	txt = fopen(ofname, "w");
	if (!txt) {
		printf("Error opening file %s.\n", ofname );
		exit(1);
	}

	fprintf( txt, "#\n" );
	fprintf( txt, "# Sample name: %.64s\n", sample_name );
	fprintf( txt, "# Sample id:   %.64s\n", sample_id );
	fprintf( txt, "# Sample type: %.16s\n", sample_type );
	fprintf( txt, "# User name:   %.24s\n", user_name );
	fprintf( txt, "# Sample description: %.256s\n", sample_desc );
	fprintf( txt, "#\n" );
	fprintf( txt, "# Start time:    %s\n", start_time_str );
	fprintf( txt, "# Real time (s): %.3f\n", real_time );
	fprintf( txt, "# Live time (s): %.3f\n", live_time );
	fprintf( txt, "#\n" );
	#if (defined _WIN32 && defined _MSC_VER)
		fprintf( txt, "# Total counts:  %I64d\n", total_counts );
	#else
		fprintf( txt, "# Total counts:  %"PRId64"\n", total_counts );
	#endif
	fprintf( txt, "#\n" );
	fprintf( txt, "# Left marker:  %d (%.3f %s)\n", marker_left, energy(marker_left, A), energy_unit );
	fprintf( txt, "# Right marker: %d (%.3f %s)\n", marker_right, energy(marker_right, A), energy_unit );
	#if (defined _WIN32 && defined _MSC_VER)
		fprintf( txt, "# Counts:  %I64d\n", marker_counts );
	#else
		fprintf( txt, "# Counts:       %"PRId64"\n", marker_counts );
	#endif
	fprintf( txt, "#\n" );
	fprintf( txt, "# Energy calibration coefficients ( E = sum(Ai * n**i) )\n" );
	for (i=0; i<4; i++)
		fprintf( txt, "#     A%d: %.6f\n", i, A[i] );
	fprintf( txt, "# Energy unit: %s\n", energy_unit );
	fprintf( txt, "#\n" );
	fprintf( txt, "# Channel data\n" );
	fprintf( txt, "# n\tenergy(%s)\tcounts\trate(1/s)\n", energy_unit );
	fprintf( txt, "#-----------------------------------------------------------------------\n" );
	/* NOTE: Genie2k starts counting channels with _1_ */
	for (i=0; i<n_channels; i++)
		fprintf( txt, "%d\t%.3f\t%lu\t%g\n", i+1, energy(i+1, A), channels[i], channels[i]/live_time );

	fclose(cnf);
	fclose(txt);
	return 0;
	}