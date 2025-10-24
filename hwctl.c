/* hwctl.c
   Simple controller:
     hwctl list         -> shows daemon state (/var/lib/hwmond/state.txt)
     hwctl list-rules   -> prints /etc/hwmond.conf
     hwctl add-fixed <target> <value>
     hwctl add-curve <target> <curve> <input>
     hwctl remove <target>
     hwctl reload
   Build: gcc -O2 -o hwctl hwctl.c
*/
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>

#define CONFIG_PATH "/etc/hwmond.conf"
#define STATE_PATH "/var/lib/hwmond/state.txt"

static void usage(const char *p){
    fprintf(stderr, "usage: %s list|list-rules|add-fixed <target> <value>|add-curve <target> <curve> <input>|remove <target>|reload\n", p);
}

static int file_exists(const char *p){ return access(p, R_OK)==0; }

static void cmd_list_state(){
    if (!file_exists(STATE_PATH)) { printf("(no state file)\n"); return; }
    FILE *f = fopen(STATE_PATH, "r");
    if (!f) { perror("open state"); return; }
    char buf[512];
    while (fgets(buf, sizeof(buf), f)) fputs(buf, stdout);
    fclose(f);
}

static void cmd_list_config(){
    if (!file_exists(CONFIG_PATH)) { printf("(no config)\n"); return; }
    FILE *f = fopen(CONFIG_PATH, "r");
    if (!f) { perror("open config"); return; }
    char buf[512];
    while (fgets(buf, sizeof(buf), f)) fputs(buf, stdout);
    fclose(f);
}

/* reload daemon by signaling it (uses pidof) */
static int reload_daemon(){
    int rc = system("pidof hwmond > /dev/null 2>&1");
    if (rc != 0) {
        fprintf(stderr, "hwmond not running (pidof failed)\n");
        return -1;
    }
    FILE *p = popen("pidof hwmond", "r");
    if (!p) return -1;
    char buf[64];
    if (!fgets(buf, sizeof(buf), p)) { pclose(p); return -1; }
    pclose(p);
    pid_t pid = (pid_t)strtol(buf, NULL, 10);
    if (pid <= 0) return -1;
    if (kill(pid, SIGHUP) != 0) { perror("kill"); return -1; }
    printf("signalled hwmond (pid %d) to reload\n", pid);
    return 0;
}

static int add_line_to_config(const char *line) {
    FILE *f = fopen(CONFIG_PATH, "a");
    if (!f) { perror("open config for append"); return -1; }
    fprintf(f, "%s\n", line);
    fclose(f);
    return 0;
}

/* remove rule lines whose first token equals target */
static int remove_target_from_config(const char *target) {
    FILE *f = fopen(CONFIG_PATH, "r");
    if (!f) { perror("open config"); return -1; }
    FILE *tmp = tmpfile();
    if (!tmp) { perror("tmpfile"); fclose(f); return -1; }
    char *line = NULL; size_t cap = 0; ssize_t len; int removed = 0;
    while ((len = getline(&line, &cap, f)) > 0) {
        char copy[1024]; strncpy(copy, line, sizeof(copy)-1); copy[sizeof(copy)-1] = 0;
        char tgt[512]; if (sscanf(copy, "%511s", tgt) == 1) {
            if (strcmp(tgt, target) == 0) { removed = 1; continue; }
        }
        fputs(line, tmp);
    }
    free(line);
    rewind(tmp);
    FILE *out = fopen(CONFIG_PATH, "w");
    if (!out) { perror("open config write"); fclose(f); fclose(tmp); return -1; }
    char buf[512];
    while (fgets(buf, sizeof(buf), tmp)) fputs(buf, out);
    fclose(out); fclose(f); fclose(tmp);
    return removed ? 0 : -1;
}

int main(int argc, char **argv) {
    if (argc < 2) { usage(argv[0]); return 1; }
    if (strcmp(argv[1], "list") == 0) {
        cmd_list_state();
        return 0;
    }
    if (strcmp(argv[1], "list-rules") == 0) {
        cmd_list_config();
        return 0;
    }
    if (strcmp(argv[1], "add-fixed") == 0) {
        if (argc < 4) { usage(argv[0]); return 1; }
        char line[1024]; snprintf(line, sizeof(line), "%s fixed %d", argv[2], atoi(argv[3]));
        if (add_line_to_config(line) == 0) printf("added\n");
        reload_daemon();
        return 0;
    }
    if (strcmp(argv[1], "add-curve") == 0) {
        if (argc < 5) { usage(argv[0]); return 1; }
        char line[1024]; snprintf(line, sizeof(line), "%s curve %s %s", argv[2], argv[3], argv[4]);
        if (add_line_to_config(line) == 0) printf("added\n");
        reload_daemon();
        return 0;
    }
    if (strcmp(argv[1], "remove") == 0) {
        if (argc < 3) { usage(argv[0]); return 1; }
        if (remove_target_from_config(argv[2]) == 0) printf("removed\n");
        else printf("not found\n");
        reload_daemon();
        return 0;
    }
    if (strcmp(argv[1], "reload") == 0) {
        return reload_daemon();
    }
    usage(argv[0]);
    return 1;
}
