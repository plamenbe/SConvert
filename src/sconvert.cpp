#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <dirent.h>
#include <unistd.h>
#include "version.h"
//#include <sys/types.h>
//#include "scandir.c"


//====== Portability definitions ========
#define _POSIX_PATH_MAX 256

#if defined(__MINGW32__) || defined(__CYGWIN__)
#define NL "\n"
#else
#define NL "\r\n"
#endif

//====== Conversion definitions ========
#define VERSION "MAJOR" STATUS_SHORT "." "MINOR" "." "BUILD" //"1.0b, Aug 2018"

#define STATION_CODE "MPU-001"
#define SAMPLING_RATE "100.0000"

#define FILE_BASE_NAME "mpu-001_"
#define TMP_FILE_NAME FILE_BASE_NAME "##.bin"
#define FILE_EXPORT_EXT "txt"

#define DATE_FORMAT "%d.%m.%Y"
#define TIME_FORMAT "%H:%M:%S"

enum time_format {UNIX, CALENDAR, FLOAT_SECS, SAMPLES};

//Options flags

time_format GPStime_format = UNIX;
time_format RTCtime_format = CALENDAR;

char field_separator = '\t';
char GPStime_export = 0;
char RTCtime_export = 1;
char Status_export = 1;
char accel_export = 1;
char gyro_export = 1;
char temp_export = 1;
char batVolt_export = 1;

int samples_counter = 0;
char buff_RTC[20];
time_t starttime;

FILE* binFile;
FILE* csvFile;

struct data_t
{
    uint32_t GPStime;
    uint32_t RTCtime;
    uint16_t Status;
    int16_t ax;
    int16_t ay;
    int16_t az;
    int16_t gx;
    int16_t gy;
    int16_t gz;
    int16_t temp;
    uint16_t batVolt;
};

struct data_t testdata = {	.GPStime=15264049,
            .RTCtime=1534844887,
             .Status=10,
              .ax=368,
               .ay=760,
                .az=16304,
                 .gx=-9,
                  .gy=-76,
                   .gz=-277,
                    .temp=12,
                     .batVolt=1431
};

typedef int (*filter_fp)(const char *path, const struct dirent *, void *);

int ag_scandir(const char *dirname,
               struct dirent ***namelist,
               filter_fp filter,
               void *baton);
// Size of file base name.
const uint8_t BASE_NAME_SIZE = sizeof(FILE_BASE_NAME) - 1;
const uint8_t FILE_NAME_DIM  = BASE_NAME_SIZE + 7;
char binName[FILE_NAME_DIM] = FILE_BASE_NAME "00.bin";
char csvName[FILE_NAME_DIM];

// Number of data records in a block.
const uint16_t DATA_DIM = (512 - 4) / sizeof(data_t);

//Compute fill so block size is 512 bytes.  FILL_DIM may be zero.
const uint16_t FILL_DIM = 512 - 4 - DATA_DIM * sizeof(data_t);

struct block_t
{
    uint16_t count;
    uint16_t overrun;
    data_t data[DATA_DIM];
    uint8_t fill[FILL_DIM];
};

// Print data header.
void printHeader(FILE* pr)
{
    fprintf(pr,"Station_code\t%s" NL,STATION_CODE);
    fprintf(pr,"Sampling_rate\t" SAMPLING_RATE NL);

    strftime(buff_RTC, sizeof buff_RTC, DATE_FORMAT, gmtime(&starttime));
    fprintf(pr,"Start_date\t%s" NL,buff_RTC);
    strftime(buff_RTC, sizeof buff_RTC, TIME_FORMAT, gmtime(&starttime));
    fprintf(pr,"Start_time\t%s" NL,buff_RTC);

    if(GPStime_export)
        fprintf(pr,"GPSTime");

    if(RTCtime_export)
        {
            if(GPStime_export)
                fprintf(pr,",Time");
            else
                fprintf(pr,"Time");
        }

    if(Status_export)
        fprintf(pr,",Status");
    if(accel_export)
        fprintf(pr,",ax,ay,az");
    if(gyro_export)
        fprintf(pr,",gx,gy,gz");
    if(temp_export)
        fprintf(pr,",temp");
    if(batVolt_export)
        fprintf(pr,",batVolt");
    fprintf(pr,NL);
}

void printHeaderGeoSIG(FILE* pr)
{
    char time_header[20] = "Time";

    fprintf(pr,"Station_code\t%s" NL,STATION_CODE);
    fprintf(pr,"Sampling_rate\t" SAMPLING_RATE NL);

    strftime(buff_RTC, sizeof buff_RTC, DATE_FORMAT, gmtime(&starttime));
    fprintf(pr,"Start_date\t%s" NL,buff_RTC);
    strftime(buff_RTC, sizeof buff_RTC, TIME_FORMAT, gmtime(&starttime));
    fprintf(pr,"Start_time\t%s" NL,buff_RTC);

    if(GPStime_export)
        fprintf(pr,"GPSTime");

    switch(RTCtime_format)
        {
        case UNIX:
            sprintf(time_header,"Time");
            break;
        case CALENDAR:
            sprintf(time_header,"Time");
            break;
        case FLOAT_SECS:
            sprintf(time_header,"Time:sec");
            break;
        case SAMPLES:
            sprintf(time_header,"Samples");
            break;
        }

    if(RTCtime_export)
        {
            if(GPStime_export)
                fprintf(pr,"\t%s", time_header);
            else
                fprintf(pr,"%s", time_header);
        }

    if(Status_export)
        fprintf(pr,"\tPPS,ticks");
    if(accel_export)
        fprintf(pr,"\tax,Units\tay,Units\taz,Units");
    if(gyro_export)
        fprintf(pr,"\tgx,Units\tgy,Units\tgz,Units");
    if(temp_export)
        fprintf(pr,"\ttemp,*C");
    if(batVolt_export)
        fprintf(pr,"\tbatVolt,Units");
    fprintf(pr, NL);
}

void convertDate(char* result, time_t timestamp, time_format format)
{
    switch(format)
        {
        case UNIX:
            sprintf(result, "%li", timestamp);
            break;
        case CALENDAR:
            strftime(result, sizeof buff_RTC, TIME_FORMAT, gmtime(&timestamp));
            break;
        case FLOAT_SECS:
            sprintf(result, "%0.4e", (double)(timestamp-starttime));
            break;
        case SAMPLES:
            sprintf(result, "%i", samples_counter);
            break;
//		DATE_FORMAT " "
        }
}

// Print a data record.
void printData(FILE* pr, data_t* data)
{

    if(GPStime_export)
        {
            convertDate(buff_RTC, (data->GPStime), GPStime_format);
            fprintf(pr,"%s",buff_RTC);
        }

    if(RTCtime_export)
        {
            if(GPStime_export)
                fputc(field_separator,pr);

            convertDate(buff_RTC, (data->RTCtime), RTCtime_format);
            fprintf(pr,"%s",buff_RTC);
        }

    if(Status_export)
        {
            fputc(field_separator,pr);
            fprintf(pr,"%i",data->Status);
        }

    if(accel_export)
        {
            fputc(field_separator,pr);
            fprintf(pr,"%i",data->ax);
            fputc(field_separator,pr);
            fprintf(pr,"%i",data->ay);
            fputc(field_separator,pr);
            fprintf(pr,"%i",data->az);
        }

    if(gyro_export)
        {
            fputc(field_separator,pr);
            fprintf(pr,"%i",data->gx);
            fputc(field_separator,pr);
            fprintf(pr,"%i",data->gy);
            fputc(field_separator,pr);
            fprintf(pr,"%i",data->gz);
        }

    if(temp_export)
        {
            fputc(field_separator,pr);
            fprintf(pr,"%0.1f",data->temp / 340 + 36.5);
        }

    if(batVolt_export)
        {
            fputc(field_separator,pr);
            fprintf(pr,"%i",data->batVolt);
        }

    fprintf(pr,NL);
}


// Convert binary file to csv file.
int binaryToCsv(FILE* inFile, FILE* outFile)
{
    block_t block;
    uint8_t lastPct = 0;

    samples_counter = 0;

    printf(": [");
    fseek(inFile, 0L, SEEK_END);
    long int sz = ftell(inFile);

    rewind(inFile);

    while (fread(&block, sizeof block, 1, inFile) == 1)
        {

            if (!samples_counter)
                {
                    starttime = block.data[0].RTCtime;
                    printHeaderGeoSIG(outFile);
                }

            uint16_t i;
            if (block.count == 0 || block.count > DATA_DIM)
                {
                    break;
                }
            if (block.overrun)
                {
                    fprintf(outFile, "OVERRUN, %i", block.overrun);
                }
            for (i = 0; i < block.count; i++)
                {
                    printData(outFile, &block.data[i]);
                    samples_counter++;
                }

            uint8_t pct = ftell(inFile) / (sz / 100);

            if ((pct != lastPct) && !(pct % 10))
                {
                    lastPct = pct;
//        printf("%d%%",pct);
                    printf("=");
                }
        }
    printf("]");
    return samples_counter;
}

void recoverTmpFile()
{
    uint16_t count;

    if ((binFile = fopen(binName,"rb+")))
        {
            fseek(binFile, 0L, SEEK_END);
            long int sz = ftell(binFile);
            rewind(binFile);

            if (fread(&count, 2, 1, binFile) != 1 || count != DATA_DIM)
                {
                    printf("Please delete existing tmp file: %s\n", binName);
                }
            printf("Recovering data in tmp file %s. ", binName);
            uint32_t bgnBlock = 0;
            uint32_t endBlock = sz / 512 - 1;
            // find last used block.
            while (bgnBlock < endBlock)
                {
                    uint32_t midBlock = (bgnBlock + endBlock + 1) / 2;
                    fseek(binFile, (512 * midBlock), SEEK_SET);
                    if (fread(&count, 2, 1, binFile) != 1)
                        printf("Error read!\n");
                    if (count == 0 || count > DATA_DIM)
                        {
                            endBlock = midBlock - 1;
                        }
                    else
                        {
                            bgnBlock = midBlock;
                        }
                }
            // truncate after last used block.
//  _chsize(fileno(binFile), 512 * (bgnBlock + 1));

            if (ftruncate(fileno(binFile), 512 * (bgnBlock + 1)) != 0)
                {
                    printf("Truncate failed!\n");
                    exit(1);
                }
        }
    fclose(binFile);
    printf("Done!\n");
}

int fileConvert()
{
    char tch[100];

    if (strstr(binName, "##.bin"))
        recoverTmpFile();

    if ((binFile = fopen(binName,"rb")))
        {
            // Create a new csvFile.
            strcpy(csvName, binName);
            strcpy(&csvName[BASE_NAME_SIZE + 3], FILE_EXPORT_EXT);

            if ((csvFile = fopen(csvName, "w")))
                {
//			printf("%s -> %s ", binName, csvName);
                    printf("%s", binName);
                    printf(" %i records -> ", binaryToCsv(binFile,csvFile));
                    fclose(csvFile);
                    strftime(buff_RTC, sizeof buff_RTC, "%Y-%m-%d_%H-%M-%S", gmtime(&starttime));
                    sprintf(tch,"%s_%s",buff_RTC,csvName);
                    int rc = rename(csvName, tch);
                    if(rc)
                        {
                            perror("rename");
                            printf("Converted file is %s\n",csvName);
                        }
                    else
                        {
                            printf("%s\n",tch);
                        }
                }
            else
                {
                    perror("File creation failed");
                    return EXIT_FAILURE;
                }
        }
    else
        {
            perror("File opening failed ");
            return EXIT_FAILURE;
        }

    fclose(binFile);
    return 0;
}

static int fileFilter (const struct dirent *fname)
{
    if (strstr(fname->d_name,".bin"))
        return 1;
    else
        return 0;
}


int alphasort(const void *d1, const void *d2)
{
    return(strcmp((*(struct dirent **)d1)->d_name, (*(struct dirent **)d2)->d_name));
}

/*
int scandir(char *dirname, struct dirent *(*namelist[]), int (*select)(struct dirent *), int (*dcomp)()){
   DIR *dirp;
   struct dirent *res;
   static struct dirent *tdir[256];
   int tdirsize = sizeof(struct dirent);
   register int i=0;

   if ((dirp = opendir(dirname)) == NULL)
     return -1;

// 256 should be big enough for a spool directory of any size
//   big enough for 128 jobs anyway.

   if ((*namelist = (struct dirent **) calloc(256, sizeof(struct dirent *))) == NULL)
         {
                 closedir(dirp);
     return -1;
         }

   if ((res = (struct dirent *) malloc(tdirsize + _POSIX_PATH_MAX)) == NULL)
         {
                 closedir(dirp);
     return -1;
         }

   while (readdir(dirp) != NULL) {
     if (select(res)) {
       if (((*namelist)[i] = (struct dirent *) malloc(tdirsize + _POSIX_PATH_MAX)) == NULL)
                         {
                                 closedir(dirp);
         return -1;
                         }
     memcpy((*namelist)[i], res, sizeof(res)+_POSIX_PATH_MAX);
     i++;
     }
   }

   if (dcomp != NULL)
     qsort((char *) &((*namelist)[0]), i, sizeof(struct dirent *), dcomp);

         closedir(dirp);
   return i;
 }
*/
void printHelp()
{
    printf("Usage:\n\tsconvert <filename> <options> /?\n");
    printf("\n\t<filename> -\tfile with extension .bin. If omitted\n\t\t\tall files in folder will be converted.\n");
}

int main(int argc, char *argv[])
{

    printf("SConvert ver. %i.%i" STATUS_SHORT " build %i %s/%s/%s\n", MAJOR, MINOR, BUILD, DATE, MONTH, YEAR);
    printf("Web: <https://github.com/plamenbe/SConvert>\n");
    printf("Program converts .bin file into GeoSIG ASCII file format.\n");
    printf("GeoSIG ASCII files can be viewed with GeoDAS software <https://www.geosig.com/>.\n\n");
    //printf("Use /? for help.\n\n");

    if (argc > 1)
        {
            if (strstr(argv[1],".bin"))
                {
                    strcpy(binName, argv[1]);
                    fileConvert();
                }
        }
    else
        {
#if defined(__MINGW32__) || defined(__CYGWIN__)
            printf("Sorry, this version does not support batch converting under Windows OS.\nTry convert file by file.");
#else
            struct dirent **eps;
            int n;

            n = scandir ("./", &eps, fileFilter, alphasort);
            if (n >= 0)
                {
                    int cnt;
                    for (cnt = 0; cnt < n; ++cnt)
                        {
                            strcpy(binName, eps[cnt]->d_name);
                            fileConvert();
//	        puts (eps[cnt]->d_name);
                        }
                }
            else
                perror ("Couldn't open the directory");
#endif
        }

    return 0;
}

/*

int ag_scandir(const char *dirname,
               struct dirent ***namelist,
               filter_fp filter,
               void *baton) {
    DIR *dirp = NULL;
    struct dirent **names = NULL;
    struct dirent *entry, *d;
    int names_len = 32;
    int results_len = 0;

    dirp = opendir(dirname);
    if (dirp == NULL) {
        goto fail;
    }

    names = malloc(sizeof(struct dirent *) * names_len);
    if (names == NULL) {
        goto fail;
    }

    while ((entry = readdir(dirp)) != NULL) {
        if ((*filter)(dirname, entry, baton) == 0) {
            continue;
        }
        if (results_len >= names_len) {
            struct dirent **tmp_names = names;
            names_len *= 2;
            names = realloc(names, sizeof(struct dirent *) * names_len);
            if (names == NULL) {
                free(tmp_names);
                goto fail;
            }
        }

#if defined(__MINGW32__) || defined(__CYGWIN__)
        d = malloc(sizeof(struct dirent));
#else
        d = malloc(entry->d_reclen);
#endif

        if (d == NULL) {
            goto fail;
        }
#if defined(__MINGW32__) || defined(__CYGWIN__)
        memcpy(d, entry, sizeof(struct dirent));
#else
        memcpy(d, entry, entry->d_reclen);
#endif

        names[results_len] = d;
        results_len++;
    }

    closedir(dirp);
    *namelist = names;
    return results_len;

fail:
    if (dirp) {
        closedir(dirp);
    }

    if (names != NULL) {
        int i;
        for (i = 0; i < results_len; i++) {
            free(names[i]);
        }
        free(names);
    }
    return -1;
}
*/
