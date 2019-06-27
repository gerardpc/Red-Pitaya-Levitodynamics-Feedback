/* *********************************************************************
 * Main C code for the CPU side of the Red Pitaya STEMLAB
 * *********************************************************************
 * COMMENTS:
 * *********************************************************************
 * VERSION:
 * 0.4
 * GP Conangla, 27.12.2018
 * *********************************************************************
 */

////////////////////////////////////////////////////////////////////////
// Libraries
#include <stdlib.h>      // general functions and variable types
#include <stdio.h>       // standard input/output
#include <stdint.h>      // more integer lengths
#include <unistd.h>      // symbolic constants/types
#include <sys/mman.h>    // memory management 
#include <fcntl.h>       // file control (open...)
#include <string.h>      // strings package
#include <math.h>        // math functions
#include <unistd.h>      // for the sleep function

////////////////////////////////////////////////////////////////////////
// Color code
#define ANSI_COLOR_RED     "\x1b[91m"
#define ANSI_COLOR_GREEN   "\x1b[92m"
#define ANSI_COLOR_YELLOW  "\x1b[93m"
#define ANSI_COLOR_BLUE    "\x1b[94m"
#define ANSI_COLOR_MAGENTA "\x1b[95m"
#define ANSI_COLOR_CYAN    "\x1b[96m"
#define ANSI_COLOR_RESET   "\x1b[0m"

////////////////////////////////////////////////////////////////////////
// Functions


// Matrix transpose function
//
int matrix_transpose(double matrix[2][2], double m_transpose[2][2]){
    // swap matrix[0][1] and matrix[1][0]
    // set values on inverse matrix
    m_transpose[0][0] = matrix[0][0];
    m_transpose[0][1] = matrix[1][0];
    m_transpose[1][0] = matrix[0][1];
    m_transpose[1][1] = matrix[1][1];
    
    // end function normally
    return 0;
}


// Matrix inverse function
//
int matrix_inverse(double matrix[2][2], double m_inverse[2][2]){
    
    // calculate determinant
    double det = matrix[0][0]*matrix[1][1] - matrix[0][1]*matrix[1][0];
    
    // set values on inverse matrix
    m_inverse[0][0] = matrix[1][1]/det;
    m_inverse[0][1] = -matrix[0][1]/det;
    m_inverse[1][0] = -matrix[1][0]/det;
    m_inverse[1][1] = matrix[0][0]/det;
    
    // end function normally
    return 0;
}

// Matrix product function
//
// Multiply mat1 by mat2 and put result in m_prod
//
 int matrix_product(int m, int n, double mat1[m][n], int p, int q, double mat2[p][q], double m_prod[m][q]) {
    int i, j, k;
    if(n != p){
        printf("Matrix dimension mismatch\n");
        // error exit
        return 1;        
    } else {
        for(i = 0; i < q; i++){
            for(j = 0; j < m; j++){
                m_prod[j][i] = 0;
                for(k = 0; k < n; k++){
                    m_prod[j][i] = m_prod[j][i] + (mat1[j][k]*mat2[k][i]);
                }
            }
        }
        // end function normally
        return 0;
    }
}

// Matrix addition function
//
// Add mat1 and mat2 and put result in m_add
//
int matrix_addition(double mat1[2][2], double mat2[2][2], double m_add[2][2]){
    // add matrices and put result in m_add
    m_add[0][0] = mat1[0][0] + mat2[0][0];
    m_add[0][1] = mat1[0][1] + mat2[0][1];
    m_add[1][0] = mat1[1][0] + mat2[1][0];
    m_add[1][1] = mat1[1][1] + mat2[1][1];
    
    // end function normally
    return 0;
}

// Matrix multiply scalar
//
int m_mult_scalar(double matrix[2][2], double scalar){
    // Matrix multiply by scalar
    matrix[0][0] = matrix[0][0]*scalar;
    matrix[0][1] = matrix[0][1]*scalar;
    matrix[1][0] = matrix[1][0]*scalar;
    matrix[1][1] = matrix[1][1]*scalar;
    
    // end function normally
    return 0;
}

// Print matrix function
//
int print_matrix(int rows, int columns, double matrix[rows][columns]){
    for(int i = 0; i < rows; i++){
        for(int j = 0; j < columns; j++){
            printf("%.3e ", matrix[i][j]);
        }
        printf("\n");
    }
    // end function normally
    return 0;
}

// Saturate gain
//
int sat_gain(long int k, short int *k_short){
    long int max_gain = 8191;
    if(k > max_gain){
        *k_short = max_gain;
    } else if(k < -max_gain){
        *k_short = -max_gain;
    } else {
        *k_short = (short int) k;
    }
    // end normally
    return 0;
}

// Saturate delay
//
int sat_delay(long int delay, short int *delay_short){
    long int max_delay = 500;
    if(delay > max_delay){
        *delay_short = max_delay;
    } else if(delay < 0){
        *delay_short = 0;
    } else {
        *delay_short = (short int) delay;
    }
    // end normally
    return 0;
}


// floating point to fxp
/*
int float_to_fxp(double input, short int *output){
    double tmp = input*65534;
    output = (short int) round(tmp);
    // end normally
    return 0;
}*/

////////////////////////////////////////////////////////////////////////
// Main loop
//
int main(int argc, char **argv){
    
    ////////////////////////////////////////////////////////////////////
    // Program variables
    char input_data[256]; // keyboard input
    void *cfg_delay; // pointer to delay register in virtual memory
    void *cfg_pid; // pointer to pid register in virtual memory
    void *data_energy; // pointer to particle energy in virtual memory
    int fd; // file identifier
    
    ////////////////////////////////////////////////////////////////////
    // Feedback variables
    uint32_t reg_delay; //delay register value
    uint32_t reg_pid; //pid register value
    uint32_t reg_energy; //register energy value
    uint32_t reg_tmp1; //temporary parameter
    uint32_t reg_tmp2; //temporary parameter
    
    long int delay_long; // delay cycles
    short int delay = 0; // delay cycles
    long int k_p_long; // k_p in PID 
    long int k_d_long; // k_d in PID    
    short int k_p; // k_p in PID 
    short int k_d; // k_d in PID
    int energy_int; // current particle energy from out of loop detector.
    float energy; // current particle energy from out of loop detector.
    
    ////////////////////////////////////////////////////////////////////
    // Open virtual memory
    fd = open("/dev/mem", O_RDWR); // file identifier
    if(fd < 0) {
        perror("open");
        return 1; // return error in opening
    }    
    // Let the program manipulate virtual memory
    // 0x........ is the address of fpga gpio register
    cfg_delay   = mmap(NULL, sysconf(_SC_PAGESIZE), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0x41200000);
    data_energy = cfg_delay + 8;
    cfg_pid     = mmap(NULL, sysconf(_SC_PAGESIZE), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0x42000000);
            
    ////////////////////////////////////////////////////////////////////
    // Print welcome message
    printf("\n");
    printf(ANSI_COLOR_RED "##########################################################\n");
    printf("------------------------\n");
    printf("| LQG control software |\n");
    printf("------------------------\n"); 
    printf("\n");
    printf("Version: 0.1, 12.12.18, GP Conangla\n");
    printf("\n");    
    printf("------------------------\n"); 
    printf("LQG optimal control of an optically levitated particle.\n");
    printf("\n");
    printf("Steps of the digital signal processing:\n");
    printf("1. Read position data from a trapped particle\n");
    printf("2. Apply LQR (optimal feedback control)\n");
    printf("------------------------\n"); 
    printf("##########################################################\n" ANSI_COLOR_RESET);
    
    // get current delay values
    reg_delay = *(uint32_t *)(cfg_delay);  // get register value
    delay = reg_delay & 0x0000FFFF; // delay value
    
    // get current k_p, k_d values
    reg_pid = *(uint32_t *)(cfg_pid);  // get register value
    k_p = (reg_pid << 16)>>16; // 16 lowest bits: k_p
    k_d = (reg_pid >> 16); // 16 highest bits: k_d
        
    printf("------------------------\n"); 
    printf("Current settings:\n"); 
    printf("------------------------\n"); 
    printf("Current value of delay: %d\n", delay);
    printf("------------------------\n"); 
    printf("Current value of k_p:   %d\n", k_p);
    printf("Current value of k_d:   %d\n", k_d);
    printf("------------------------\n");
    printf("\n");    
                               
    ////////////////////////////////////////////////////////////////////
    // Enter loop
    do{    
        // what do you want to do next?    
        printf("Type...\n");
        printf("    'p' to print current settings (k_p, k_d, delay)\n");
        printf("    'delay' to configure the total delay,\n");
        printf("    'f' to configure the feedback parameters,\n");
        printf("    'ml' to start a ML routine and optimize k_d step by step,\n");
        printf("    'mlauto' to start a ML routine and optimize kd automatically,\n");
        printf("    'k' to kill (i.e. stop) the feedback!,\n");
        printf("    'exit' to quit\n>> ");
        
        // check if input works
        if (fgets(input_data, sizeof(input_data), stdin) != NULL) {
            // input has worked, do something with data
            printf("\n");
            input_data[strlen(input_data)-1]='\0'; // remove newline character from string
            
            // "p" case -> print delay, kp, kd
            if(0 == strcmp(input_data, "p")){                    
                // print current k_p, k_d values
                printf("\n"); 
                printf("------------------------\n"); 
                printf("Current settings:\n"); 
                printf("------------------------\n"); 
                printf("Current value of delay: %d\n", delay);
                printf("------------------------\n"); 
                printf("Current value of k_p:   %d\n", k_p);
                printf("Current value of k_d:   %d\n", k_d);
                printf("------------------------\n");
                printf("\n"); 
            }   
            
            // "exit" case -> leave loop
            if(0 == strcmp(input_data, "exit")){
                break;
            }
            
            ////////////////////////////////////////////////////////////
            // "delay" case -> go to delay settings
            if(0 == strcmp(input_data, "delay")){
                // Read number of delay cycles you want
                printf("Write number of clock cycles to delay x(t)\n>> ");                
                scanf("%ld", &delay_long);
                getchar();
                printf("\n");
                sat_delay(delay_long, &delay);
                reg_delay = (unsigned int) delay & 0x0000FFFF;
                // change delay by changing register on FPGA
                *((uint32_t *)(cfg_delay)) = (uint32_t) reg_delay; 
            }
            
            ////////////////////////////////////////////////////////////
            // "feedback" case -> go to feedback settings
            if(0 == strcmp(input_data, "f")){
                // What do you want to do next?
                // exit or change feedback gains?
                printf("Type...\n");
                printf("    'c' to change values of k_p, k_d,\n");
                printf("    'exit' to end execution\n>> ");       
                
                // check if input works 
                if (fgets(input_data, sizeof(input_data), stdin) != NULL) {
                    // input has worked, do something with data
                    printf("\n");
                    input_data[strlen(input_data)-1]='\0'; // remove newline character from string
                    
                    // "exit" case -> leave loop
                    if(0 == strcmp(input_data, "exit")){
                        break;
                    }
                    
                    // "change" case -> change kp, kd
                    if(0 == strcmp(input_data, "c")){
                        printf("Type value of 'k_p'\n>> ");
                        scanf("%ld", &k_p_long);
                        getchar();
                        sat_gain(k_p_long, &k_p);
                        printf("\n");
                        printf("Type value of 'k_d'\n>> ");  
                        scanf("%ld", &k_d_long);
                        getchar();
                        sat_gain(k_d_long, &k_d);
                        printf("\n");
                                        
                        ////////////////////////////////////////////////////////////////////
                        // Manipulate/read values of registers
                        reg_tmp1 = (unsigned int) k_d << 16;
                        reg_tmp2 = (unsigned int) k_p & 0x0000FFFF;
                        reg_tmp2 = reg_tmp1 + reg_tmp2;
                        *((uint32_t *)(cfg_pid)) = (uint32_t) reg_tmp2;   // write reg_tmp2 on pid memory address
                        
                        // get current k_p, k_d values
                        reg_pid = *(uint32_t *)(cfg_pid);  // get register value
                        k_p = (int) (reg_pid << 16)>>16; // 16 lowest bits: k_p
                        k_d = (int) (reg_pid >> 16); // 16 highest bits: k_d
                        k_d = (signed int)(k_d);
                            
                        // print current k_p, k_d values
                        printf("------------------------\n");
                        printf("New value of k_p: %d\n", k_p);
                        printf("New value of k_d: %d\n", k_d);
                        printf("------------------------\n");
                        printf("\n");                             
                    }
                }
            }
            
            ////////////////////////////////////////////////////////////
            // "ml" case -> start machine learning routine to optimize kd
            if(0 == strcmp(input_data, "ml")){
                // read energy register
                reg_energy = *(uint32_t *)(data_energy);  // get register value
                energy_int = (int) reg_energy & 0x0000FFFF;
                energy = (float) energy_int;
                
                ////////////////////////////////////////////////////////////////////
                // Machine learning routine
                // tmp variable        
                reg_tmp1 = (unsigned int) 0 << 16;
                
                float k0 = -1;
                float k1 = -5;
                float k_tmp;
                
                float energy0, energy1;
            
                // Modify k_d value with k0
                int k0_int = (int) k0;
                reg_tmp1 = (unsigned int) k0_int << 16;
                *((uint32_t *)(cfg_pid)) = (uint32_t) reg_tmp1;                
                printf("Initial kd = %.1f\n", k0); 
                                
                usleep(3000000);      
                // read energy register
                reg_energy = *(uint32_t *)(data_energy);  // get register value
                energy_int = (int) reg_energy & 0x0000FFFF;
                energy0 = (float) energy_int;
                energy1 = energy0 + 2;
                
                printf("------------------------\n");
                printf("Initial energy value: %.1f\n", energy0); 
                printf("------------------------\n\n");                      
                printf(ANSI_COLOR_RED "##########################################################\n" ANSI_COLOR_RESET);               
                printf("------------------------\n");
                printf("Next value of k_d: %d\n", (int) k1);
                printf("------------------------\n");
                
                printf("Press any key to continue\n");  
                getchar();    
                
                do{   
                    // Modify k_d value with k1
                    int k1_int = (int) k1;
                    reg_tmp1 = (unsigned int) k1_int << 16;
                    *((uint32_t *)(cfg_pid)) = (uint32_t) reg_tmp1; 
                    // get current k_d value
                    reg_pid = *(uint32_t *)(cfg_pid);  // get register value
                    k_d = (int) (reg_pid >> 16);       // 16 highest bits: k_d
                    k_d = (signed int)(k_d);                    
                                    
                    usleep(3000000);      
                    // get new energy from register
                    energy0 = energy1;
                    reg_energy = *(uint32_t *)(data_energy);  // get register value
                    energy_int = (int) reg_energy & 0x0000FFFF;
                    energy1 = (float) energy_int;
                    
                    printf("------------------------\n");
                    printf("Updated energy value: %.1f\n", energy1); 
                    printf("------------------------\n"); 
                    printf("Energy difference: %.1f\n", fabs(energy1 - energy0)); 
                    printf("------------------------\n\n"); 
                    
                    // gradient descent
                    k_tmp = k1;
                    float dif_k = k1 - k0;
                    if(dif_k == 0){
                        dif_k = 1;
                    }
                    k1 = k1 - 0.2*(energy1 - energy0)/(dif_k);
                    k0 = k_tmp;
                                        
                    printf(ANSI_COLOR_RED "##########################################################\n" ANSI_COLOR_RESET);
                    printf("------------------------\n");
                    printf("Next value of k_d will be: %.1f\n", k1);
                    printf("------------------------\n");
                    
                    // just in case k1 is too large
                    if(k1 > 0){
                        k1 = 0;
                    } else if(k1 < -1000){
                        k1 = -1000;
                    }
                    
                    printf("Press any key to continue\n");  
                    getchar();    
                } while(1);
                
                // get current k_d value
                reg_pid = *(uint32_t *)(cfg_pid);  // get register value
                k_d = (int) (reg_pid >> 16);       // 16 highest bits: k_d
                k_d = (signed int)(k_d);
                
                // read energy register
                reg_energy = *(uint32_t *)(data_energy);  // get register value
                energy_int = (int) reg_energy & 0x0000FFFF;
                energy = (float) energy_int;
                
                // Print final energy and k_d
                printf("------------------------\n");
                printf("Final energy value: %.1f\n", energy); 
                printf("------------------------\n");
                printf("Final value of k_d: %d\n", k_d);
                printf("------------------------\n");
                printf("\n");  
            }
            
            ////////////////////////////////////////////////////////////
            // "mlauto" case -> start machine learning routine to optimize kd
            if(0 == strcmp(input_data, "mlauto")){
                // read energy register
                reg_energy = *(uint32_t *)(data_energy);  // get register value
                energy_int = (int) reg_energy & 0x0000FFFF;
                energy = (float) energy_int;
                
                ////////////////////////////////////////////////////////////////////
                // Machine learning routine
                // tmp variable        
                reg_tmp1 = (unsigned int) 0 << 16;
                
                float k0 = -1;
                float k1 = -5;
                float k_tmp;
                
                float energy0, energy1;
            
                // Modify k_d value with k0
                int k0_int = (int) k0;
                reg_tmp1 = (unsigned int) k0_int << 16;
                *((uint32_t *)(cfg_pid)) = (uint32_t) reg_tmp1;                
                printf("Initial kd = %.1f\n", k0); 
                
                usleep(3000000);                
                // read energy register
                reg_energy = *(uint32_t *)(data_energy);  // get register value
                energy_int = (int) reg_energy & 0x0000FFFF;
                energy0 = (float) energy_int;                
                energy1 = energy0 + 2;
                
                printf("------------------------\n");
                printf("Initial energy value: %.1f\n", energy0); 
                printf("------------------------\n\n"); 
                printf(ANSI_COLOR_RED "##########################################################\n" ANSI_COLOR_RESET);        
                printf("------------------------\n");
                printf("Next value of k_d: %d\n", (int) k1);
                printf("------------------------\n");
                
                do{   
                    // Modify k_d value with k1
                    int k1_int = (int) k1;
                    reg_tmp1 = (unsigned int) k1_int << 16;
                    *((uint32_t *)(cfg_pid)) = (uint32_t) reg_tmp1; 
                    // get current k_d value
                    reg_pid = *(uint32_t *)(cfg_pid);  // get register value
                    k_d = (int) (reg_pid >> 16);       // 16 highest bits: k_d
                    k_d = (signed int)(k_d);       
                    
                    usleep(3000000);
                    
                    // get new energy from register
                    energy0 = energy1;
                    reg_energy = *(uint32_t *)(data_energy);  // get register value
                    energy_int = (int) reg_energy & 0x0000FFFF;
                    energy1 = (float) energy_int;

                    printf("------------------------\n");
                    printf("Updated energy value: %.1f\n", energy1); 
                    printf("------------------------\n"); 
                    printf("Energy difference: %.1f\n", fabs(energy1 - energy0)); 
                    printf("------------------------\n\n"); 
                    // gradient descent
                    k_tmp = k1;
                    float dif_k = k1 - k0;
                    if(dif_k == 0){
                        dif_k = 1;
                    }
                    k1 = k1 - 0.2*(energy1 - energy0)/(dif_k);
                    k0 = k_tmp;
                                        
                    printf(ANSI_COLOR_RED "##########################################################\n" ANSI_COLOR_RESET);
                    printf("------------------------\n");
                    printf("Next value of k_d will be: %.1f\n", k1);
                    printf("------------------------\n");
                    
                    // just in case k1 is too large
                    if(k1 > 0){
                        k1 = 0;
                    } else if(k1 < -1000){
                        k1 = -1000;
                    }
                    
                    usleep(3000000);
                } while(fabs(energy1 - energy0) > 1);
                
                // get current k_d value
                reg_pid = *(uint32_t *)(cfg_pid);  // get register value
                k_d = (int) (reg_pid >> 16);       // 16 highest bits: k_d
                k_d = (signed int)(k_d);
                
                // read energy register
                reg_energy = *(uint32_t *)(data_energy);  // get register value
                energy_int = (int) reg_energy & 0x0000FFFF;
                energy = (float) energy_int;
                
                // Print final energy and k_d
                printf("------------------------\n");
                printf("Final energy value: %.1f\n", energy); 
                printf("------------------------\n");
                printf("Final value of k_d: %d\n", k_d);
                printf("------------------------\n");
                printf("\n");  
            }
            
            ////////////////////////////////////////////////////////////
            // "kill" case -> set kp, kd to zero
            if(0 == strcmp(input_data, "k")){
                ////////////////////////////////////////////////////////////////////
                // Manipulate/read values of registers
                reg_tmp1 = (unsigned int) 0 << 16;
                reg_tmp2 = (unsigned int) 0 & 0x0000FFFF;
                reg_tmp2 = reg_tmp1 + reg_tmp2;
                *((uint32_t *)(cfg_pid)) = (uint32_t) reg_tmp2;   // write reg_tmp2 on pid memory address
                
                // get current k_p, k_d values
                reg_pid = *(uint32_t *)(cfg_pid);  // get register value
                k_p = (int) (reg_pid << 16)>>16; // 16 lowest bits: k_p
                k_d = (int) (reg_pid >> 16); // 16 highest bits: k_d
                k_d = (signed int)(k_d);
                    
                // print current k_p, k_d values
                printf("------------------------\n");
                printf("New value of k_p: %d\n", k_p);
                printf("New value of k_d: %d\n", k_d);
                printf("------------------------\n");
                printf("\n");      
            }
        }
    } while(1);    
    
    ////////////////////////////////////////////////////////////////////
    // End routine    
    // Stop the program from manipulating virtual memory
    munmap(cfg_delay, sysconf(_SC_PAGESIZE));
    munmap(cfg_pid, sysconf(_SC_PAGESIZE));
    // End routine normally
    return 0;
}
