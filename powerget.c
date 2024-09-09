#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <getopt.h>
#include <time.h>
#include <stdbool.h>
#define N 1024
typedef unsigned long long uint64_t;
typedef unsigned int uint32_t;

static char buf[N];
typedef struct rapl_power_unit{
    double PU;       //power units
    double ESU;      //energy status units
    double TU;       //time units
} rapl_power_unit;

uint64_t rdmsr(int cpu, uint32_t reg) {
    sprintf(buf, "/dev/cpu/%d/msr", cpu);
    int msr_file = open(buf, O_RDONLY);
    if (msr_file < 0) {
        perror(0);
        return msr_file;
    }
    uint64_t data;
    if (pread(msr_file, &data, sizeof(data), reg) != sizeof(data)) {
        fprintf(stderr, "read msr register 0x%x error.\n", reg);
        perror(0);
        return -1;
    }
    close(msr_file);
    return data;
}

rapl_power_unit get_rapl_power_unit() {
    rapl_power_unit ret;
    uint64_t data = rdmsr(0, 0x606);
    double t = (1 << (data & 0xf));
    t = 1.0 / t;
    ret.PU = t;
    t = (1 << ((data>>8) & 0x1f));
    ret.ESU = 1.0 / t;
    t = (1 << ((data>>16) & 0xf));
    ret.TU = 1.0 / t;
    return ret;
}
void get_current_time(FILE *file, bool flag) {
    // 获取当前时间
    time_t start_time = time(NULL);
    struct tm *timeinfo = localtime(&start_time);
    // 复制一份tm结构体用于修改
    struct tm gm_time = *timeinfo;
    // 由于北京时间是UTC+8，需要手动调整
    gm_time.tm_hour += 8;
    // 转换为世界协调时间（UTC）
    time_t gm_time_t = mktime(&gm_time);
    // 再次转换为tm结构体
    timeinfo = localtime(&gm_time_t);
    // 格式化时间字符串
    char time_str[100];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", timeinfo);
    // 写入时间到文件
    if(flag)
        fprintf(file, "Start at Beijing Time:  %s\n", time_str);
    else
        fprintf(file, "Ending at Beijing Time: %s\n", time_str);
}

double* get_cpu_power(int n, int* cpus, double energy_units, int cycle) {   //cycle for ms
    int i;
    uint64_t data;
    double *st, *en, *count;
    st = malloc(n*sizeof(double));
    en = malloc(n*sizeof(double));
    count = malloc(n*sizeof(double));
    for (i=0; i<n; ++i) {
        data = rdmsr(cpus[i], 0x611);
        st[i] = (data & 0xffffffff) * energy_units;
    }
    // 停顿cycle*1000微秒来让cpu消耗能量
    usleep(cycle*1000);
    for (i=0; i<n; ++i) {
        data = rdmsr(cpus[i], 0x611);
        en[i] = (data & 0xffffffff) * energy_units;
        count[i] = 0;
        if (en[i] < st[i]) {
            count[i] = (double)(1ll << 32) + en[i] - st[i];
        }
        else {
            count[i] = en[i] - st[i];
        }
        // 转为每秒的消耗 J
        count[i] = count[i] / ((double)cycle) * 1000.0;
    }
    for (i=0; i<n; ++i) {
        printf("get cpu %d power comsumption is: %f J/s\n", cpus[i], count[i]);
    }

    free(st);
    free(en);
    return count;
}

struct option longOpt[] = {
    {"ip", required_argument, NULL, 'i'},
    {"time", required_argument, NULL, 't'},
    {"cpu", required_argument, NULL, 'c'},
    {"help", no_argument, NULL, 'h'},
    {NULL, 0, NULL, 0}
};
static const char* optString = "i:t:h";
void usage(const char* program_name) {
    printf("usage: %s option argument\n", program_name);
    printf("       -i --ip [ip] set the IP address.\n");
    printf("       -t --time [time] set the cycle of get power, unit is ms.\n");
    printf("       -h --help print this help infomation.\n");
    printf("       This program is used to calculate the total power comsumption in cpu list. This program needs root privillege.\n");
    exit(0);
}

static int cpus[96]; // monitor V-cpu, {0,48} belongs to node0; {24,72} belongsto node1
int main(int argc, char** argv) {
    int opt, longIndex;
    int cycle;
    int n = 96; // Set the number of CPUs to 96 (0-95)
    char* ip = NULL; // IP address
    int i;

    for(int i = 0;i < n; i++) {
        cpus[i] = i;
    }

    opt = getopt_long(argc, argv, optString, longOpt, &longIndex);
    if (opt == -1) {
        usage(argv[0]);
    }
    while (opt != -1) {
        switch(opt) {
            case 'i':
                ip = optarg;
                break;
            case 't':
                cycle = atoi(optarg);
                // printf("prim cycle = %d\n", cycle);
                break;
            case 'h':
            case 0:
            default:
                usage(argv[0]);
        }
        opt = getopt_long(argc, argv, optString, longOpt, &longIndex);
    }

    if (ip == NULL) {
        fprintf(stderr, "Error: IP address is required.\n");
        usage(argv[0]);
    }

    FILE* file = fopen("consumption.log", "a");
    if (file == NULL) {
        perror("Error opening file");
        return 1;
    }

    // 记录开始时间
    get_current_time(file, true);
    
    fprintf(file, "grabing IP address: %s\n", ip);
    rapl_power_unit power_units = get_rapl_power_unit();
    double* power_consumption = get_cpu_power(n, cpus, power_units.ESU, cycle);
    double sum_0_23 = 0.0, sum_24_47 = 0.0, sum_48_71 = 0.0, sum_72_95 = 0.0;
    for (int i = 0; i < n; i++) {
        if (i>=0 && i <24) {
            sum_0_23 += power_consumption[i];
        }
        else if (i>=24 && i <48) {
            sum_24_47 += power_consumption[i];
        }
        else if (i>=48 && i <72) {
            sum_48_71 += power_consumption[i];
        }
        else if (i>=72 && i <95) {
            sum_72_95 += power_consumption[i];
        }
    }

    fprintf(file, "Sum of power consumption on Node_0 for CPUs 0  to 23: %f J/s\n", sum_0_23 / 24.0);
    fprintf(file, "Sum of power consumption on Node_1 for CPUs 23 to 47: %f J/s\n", sum_24_47 / 24.0);
    fprintf(file, "Sum of power consumption on Node_0 for CPUs 48 to 71: %f J/s\n", sum_48_71 / 24.0);
    fprintf(file, "Sum of power consumption on Node_1 for CPUs 72 to 95: %f J/s\n", sum_72_95 / 24.0);
    // 记录结束时间
    get_current_time(file, false);
    fprintf(file, "detecting duration time is %d s\n", cycle/1000);
    fprintf(file, "====================================================================\n");
    fclose(file);
    
    return 0;
}