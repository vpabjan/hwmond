/* hwmond.c
   Minimal hwmon daemon (modular rules)
   Config: /etc/hwmond.conf
   State:  /var/lib/hwmond/state.txt
   Build: gcc -O2 -o hwmond hwmond.c
*/
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <math.h>

#define CONFIG_PATH "/etc/hwmond.conf"
#define STATE_PATH "/var/lib/hwmond/state.txt"
#define POLL_SEC 2
#define MAXRULES 256

volatile sig_atomic_t reload_req = 0;
void handle_sighup(int sig){ (void)sig; reload_req = 1; }

struct Rule {
    char target[512];   // file to write (pwm)
    int mode;           // 0=fixed,1=curve
    int fixed;          // 0-255
    char curve[64];     // curve name
    char input[512];    // input sensor file (for curve)
    int last_applied;   // cached last value

	/* trigger fields */
	char operator;      // '>', '<', '='
	int trig_value;     // threshold to compare
	char trig_cmd[512]; // command to exec
    
};

static struct Rule rules[MAXRULES];
static int nrules = 0;

/* read a sensor file that usually contains either millidegrees or degrees */
static float read_input_float(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return -1.0f;
    long v = 0;
    if (fscanf(f, "%ld", &v) != 1) { fclose(f); return -1.0f; }
    fclose(f);
    if (v > 1000) return (float)v / 1000.0f;
    return (float)v;
}

static int write_int_to_file(const char *path, int v) {
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    int r = fprintf(f, "%d\n", v);
    fclose(f);
    return (r < 0) ? -1 : 0;
}

/* curves: return 0..255 */
static int curve_silent(float T){
    const float Tmin=30.0f, Tmax=80.0f;
    if (T <= Tmin) return 0;
    if (T >= Tmax) return 255;
    float x = (T - Tmin) / (Tmax - Tmin);
    float y = x*x*x;
    return (int)(y * 255.0f + 0.5f);
}
static int curve_loud(float T){
    const float Tmin=30.0f, Tmax=100.0f;
    if (T <= Tmin) return 0;
    if (T >= Tmax) return 255;
    float x = (T - Tmin) / (Tmax - Tmin);
    float y = x*x;
    return (int)(y * 255.0f + 0.5f);
}
static int curve_aggressive(float T){
    const float Tmin=30.0f, Tmax=70.0f;
    if (T <= Tmin) return 0;
    if (T >= Tmax) return 255;
    float x = (T - Tmin) / (Tmax - Tmin);
    float y = sqrtf(x);
    return (int)(y * 255.0f + 0.5f);
}
static int curve_logarithmic(float T){
    const float Tmin=30.0f, Tmax=90.0f;
    if (T <= Tmin) return 0;
    if (T >= Tmax) return 255;
    float x = (T - Tmin) / (Tmax - Tmin);
    float y = logf(1.0f + 9.0f * x) / logf(10.0f);
    return (int)(y * 255.0f + 0.5f);
}
static int curve_step(float T){
    if (T < 40.0f) return 0;
    if (T < 50.0f) return 100;
    if (T < 60.0f) return 150;
    if (T < 70.0f) return 200;
    return 255;
}

/* dispatch */
static int apply_curve_by_name(const char *name, float T){
    if (strcmp(name, "silent") == 0) return curve_silent(T);
    if (strcmp(name, "loud") == 0) return curve_loud(T);
    if (strcmp(name, "aggressive") == 0) return curve_aggressive(T);
    if (strcmp(name, "log") == 0 || strcmp(name, "logarithmic") == 0) return curve_logarithmic(T);
    if (strcmp(name, "step") == 0) return curve_step(T);
    /* default linear */
    const float Tmin=30.0f, Tmax=100.0f;
    if (T <= Tmin) return 0;
    if (T >= Tmax) return 255;
    float x = (T - Tmin) / (Tmax - Tmin);
    return (int)(x * 255.0f + 0.5f);
}

/* load a simple line-based config:
   lines: <target> fixed <value>
          <target> curve <curve_name> <input_path>
   comment lines start with '#'
*/
static void load_config(const char *path){
    FILE *f=fopen(path,"r"); if(!f){ nrules=0; return; }
    char *line=NULL; size_t cap=0; ssize_t len; int idx=0;
    while((len=getline(&line,&cap,f))>0 && idx<MAXRULES){
        if(line[len-1]=='\n') line[len-1]=0;
        char *s=line; while(*s==' '||*s=='\t') s++; if(*s=='#'||*s==0) continue;

        char target[512], mode[64];
        if(sscanf(s,"%511s %63s", target, mode)<2) continue;

        struct Rule r; memset(&r,0,sizeof(r)); r.last_applied=-1;
        strncpy(r.target,target,sizeof(r.target)-1);

        if(strcmp(mode,"fixed")==0){
            int v; if(sscanf(s,"%*s %*s %d",&v)==1){ r.mode=0; r.fixed=v; }
            else continue;
        }
        else if(strcmp(mode,"curve")==0){
            char curve[64], input[512]; 
            if(sscanf(s,"%*s %*s %63s %511s", curve, input)==2){
                r.mode=1; strncpy(r.curve,curve,sizeof(r.curve)-1);
                strncpy(r.input,input,sizeof(r.input)-1);
            } else continue;
        }
        else if(strcmp(mode,"trigger")==0){
            char op; int val; char cmd[512];
            if(sscanf(s,"%*s %*s %c %d %511[^\n]", &op, &val, cmd)==3){
                r.mode=2; r.operator=op; r.trig_value=val;
                strncpy(r.trig_cmd, cmd, sizeof(r.trig_cmd)-1);
            } else continue;
        } else continue;
        rules[idx++]=r;
    }
    free(line); fclose(f); nrules=idx;
}

/* write a small human state file so hwctl can read it easily */
static void write_state(){
    FILE *f=fopen(STATE_PATH,"w"); if(!f) return;
    time_t t=time(NULL); fprintf(f,"# hwmond state updated %s", ctime(&t));
    for(int i=0;i<nrules;i++){
        fprintf(f,"%s mode=%s ", rules[i].target, (rules[i].mode==0)?"fixed":(rules[i].mode==1)?"curve":"trigger");
        if(rules[i].mode==0) fprintf(f,"value=%d\n",rules[i].last_applied);
        else if(rules[i].mode==1) fprintf(f,"curve=%s input=%s value=%d\n", rules[i].curve,rules[i].input,rules[i].last_applied);
        else fprintf(f,"operator=%c value=%d cmd=%s\n", rules[i].operator,rules[i].trig_value,rules[i].trig_cmd);
    }
    fclose(f);
}

int main(){
    signal(SIGHUP,handle_sighup);
    load_config(CONFIG_PATH);

    while(1){
        if(reload_req){ load_config(CONFIG_PATH); reload_req=0; }
        for(int i=0;i<nrules;i++){
            if(rules[i].mode==0){
                int out=rules[i].fixed;
                if(rules[i].last_applied<0 || abs(rules[i].last_applied-out)>=3){
                    if(write_int_to_file(rules[i].target,out)==0) rules[i].last_applied=out;
                }
            }
            else if(rules[i].mode==1){
                float val=read_input_float(rules[i].input);
                if(val<0) continue;
                int out=apply_curve_by_name(rules[i].curve,val);
                if(rules[i].last_applied<0 || abs(rules[i].last_applied-out)>=3){
                    if(write_int_to_file(rules[i].target,out)==0) rules[i].last_applied=out;
                }
            }
            else if(rules[i].mode==2){
                float val=read_input_float(rules[i].target);
                int trig=0;
                if(rules[i].operator=='>') trig=(val>rules[i].trig_value);
                else if(rules[i].operator=='<') trig=(val<rules[i].trig_value);
                else if(rules[i].operator=='=') trig=((int)val==rules[i].trig_value);
                if(trig) system(rules[i].trig_cmd);
            }
        }
        write_state();
        sleep(POLL_SEC);
    }
    return 0;
}
