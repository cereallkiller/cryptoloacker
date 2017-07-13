#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "cipher.h"
#include "common.h"

char aux_log[250];

int cipher(char *input_file, char *output_file, unsigned int key, char *logger) {

        int par_for = 0;

        // input file
        int input = open(input_file, O_RDONLY, 0);
        if (input == -1) {
                sprintf(logger, "Input file \"%s\" cannot be read.", input_file);
                return RTRN_NOK;
        }
        int input_lock = flock(input, LOCK_SH);
        if (input_lock != 0) {
                sprintf(logger, "Unable to get lock on input file \"%s\".", input_file);
                close(input);
                return RTRN_TRNS_NOK;
        }
        int input_size = lseek(input, 0, SEEK_END);
        if (input_size == -1) {
                sprintf(logger, "lseek error on input file \"%s\".", input_file);
                close(input);
                return RTRN_NOK;
        }
        int *input_map = mmap(NULL, input_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_POPULATE, input, 0);
        if (input_map == MAP_FAILED) {
                sprintf(logger, "Unable to memory-map input file \"%s\".", input_file);
                close(input);
                return RTRN_NOK;
        }

        // output file
        int output = open(output_file, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        if (output == -1) {
                sprintf(logger, "Output file \"%s\" cannot be written.", output_file);
                close(input);
                return RTRN_NOK;
        }
        int output_size = lseek(output, input_size-1, SEEK_SET);
        if (output_size == -1) {
                sprintf(logger, "lseek error on output file \"%s\".", output_file);
                close(output);
                close(input);
                return RTRN_NOK;
        }
        int result = write(output, "", 1);
        if (result < 0) {
                sprintf(logger, "Unable to write last byte of output file \"%s\".", output_file);
                close(output);
                close(input);
                return RTRN_NOK;
        }

        int *output_map = mmap(NULL, input_size-1, PROT_READ | PROT_WRITE, MAP_SHARED, output, 0);
        if (output_map == MAP_FAILED) {
                sprintf(logger, "Unable to memory-map output file \"%s\".", output_file);
                close(output);
                close(input);
                return RTRN_NOK;
        }

        // parallelism needed
        if (input_size > 1024*PAR_FOR_AT_K) {
                par_for = 1;
        }

        //XOR data and write it to file
        int i;
        int upperbound = ceil(input_size/4.);
        if (par_for == 1)
        #pragma omp parallel for
        for(i = 0; i < upperbound; i++) {
                output_map[i] = input_map[i] ^ key;
        }

        // for(int byte_ctr = 0; byte_ctr < st.st_size; byte_ctr += 4) {
        //         read_bytes = fread(encrypt_bytes, 1, 4, input);
        //         for(int byte_ctr_sub = 0; byte_ctr_sub < read_bytes; byte_ctr_sub++) {
        //                 if (encrypt_bytes[byte_ctr_sub] == EOF) {
        //                         continue;
        //                 }
        //
        //                 fputc(encrypt_bytes[byte_ctr_sub] ^ key_str[key_count], output);
        //
        //                 //Increment key_count and start over if necessary
        //                 key_count++;
        //                 if (key_count == strlen(key_str)) {
        //                         key_count = 0;
        //                 }
        //         }
        // }

        //Close files
        int input_unmap = munmap(input_map, input_size);
        if (input_unmap < 0) {
                sprintf(logger, "Something wrong while memory-unmapping input file \"%s\".", input_file);
        }
        close(input);
        close(input_lock);

        int output_unmap = munmap(output_map, input_size-1);
        if (output_unmap < 0) {
                sprintf(logger, "Something wrong while memory-unmapping output file \"%s\".", output_file);
                close(output);
                return RTRN_NOK;
        }
        close(output);

        return RTRN_CPH_OK;
}
