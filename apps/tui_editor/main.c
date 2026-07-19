#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "des/des.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_INPUT 128
#define COLOR_DEFAULT (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE)
#define COLOR_GREEN   (FOREGROUND_GREEN | FOREGROUND_INTENSITY)
#define COLOR_RED     (FOREGROUND_RED | FOREGROUND_INTENSITY)
#define COLOR_CYAN    (FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY)
#define COLOR_YELLOW  (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY)
#define COLOR_DIM     (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE)
#define COLOR_WHITE   (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY)
#define COLOR_SELECT  (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY | BACKGROUND_BLUE)

static HANDLE hConsole;
static int screenW, screenH;

typedef enum {
    PAGE_RESOURCES,
    PAGE_STAGES,
    PAGE_ARRIVALS,
    PAGE_SIM,
    PAGE_VIEW,
    PAGE_COUNT
} Page;

static const char *pageNames[] = {
    "Resources", "Stages", "Arrivals", "Simulation", "View/Save"
};

static void console_init(void) {
    hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hConsole, &csbi);
    screenW = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    screenH = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    CONSOLE_CURSOR_INFO ci = { 1, TRUE };
    SetConsoleCursorInfo(hConsole, &ci);
}

static void goto_xy(int x, int y) {
    COORD pos = { (SHORT)x, (SHORT)y };
    SetConsoleCursorPosition(hConsole, pos);
}

static void set_color(WORD color) {
    SetConsoleTextAttribute(hConsole, color);
}

static void clear_screen(void) {
    system("cls");
}

static void draw_tabs(int active) {
    goto_xy(0, 0);
    set_color(COLOR_WHITE);
    printf(" DES Config Editor  ");
    set_color(COLOR_DIM);
    printf("| ");
    for (int i = 0; i < PAGE_COUNT; i++) {
        if (i == active) {
            set_color(COLOR_SELECT);
            printf(" %s ", pageNames[i]);
            set_color(COLOR_DIM);
        } else {
            printf(" %s ", pageNames[i]);
        }
        printf("| ");
    }
    printf("\n");
    set_color(COLOR_DIM);
    for (int i = 0; i < screenW; i++) putchar('-');
    set_color(COLOR_DEFAULT);
}

static void draw_footer(void) {
    int y = screenH - 2;
    goto_xy(0, y);
    set_color(COLOR_DIM);
    for (int i = 0; i < screenW; i++) putchar('-');
    goto_xy(0, y + 1);
    set_color(COLOR_DIM);
    printf(" Tab:switch  Up/Down:select  Enter/E:edit  A:add  D:delete  Ctrl+S:save  Ctrl+O:load  Esc:quit");
}

static int prompt_line(int x, int y, const char *label, char *buf, int bufsize, WORD color) {
    goto_xy(x, y);
    set_color(color);
    printf("%-20s", label);
    set_color(COLOR_WHITE);
    for (int i = 0; i < bufsize; i++) putchar('_');
    goto_xy(x + 20, y);

    int len = (int)strlen(buf);
    printf("%s", buf);

    INPUT_RECORD ir;
    DWORD count;
    int pos = len;

    while (1) {
        while (PeekConsoleInput(hConsole, &ir, 1, &count) && count > 0) {
            ReadConsoleInput(hConsole, &ir, 1, &count);
            if (ir.EventType != KEY_EVENT || !ir.Event.KeyEvent.bKeyDown) continue;

            WORD vk = ir.Event.KeyEvent.wVirtualKeyCode;
            CHAR ch = ir.Event.KeyEvent.uChar.AsciiChar;

            if (vk == VK_RETURN) {
                buf[pos] = '\0';
                return 1;
            } else if (vk == VK_ESCAPE) {
                return 0;
            } else if (vk == VK_BACK && pos > 0) {
                pos--;
                buf[pos] = '\0';
                goto_xy(x + 20 + pos, y);
                printf(" ");
                goto_xy(x + 20 + pos, y);
            } else if (ch >= 32 && ch < 127 && pos < bufsize - 1) {
                buf[pos++] = ch;
                buf[pos] = '\0';
                goto_xy(x + 20 + pos - 1, y);
                set_color(COLOR_WHITE);
                printf("%c", ch);
            }
        }
        Sleep(20);
    }
}

/* --- Draw pages with selection highlight --- */

static void draw_resources_page(DesSimConfig *cfg, int selected) {
    int y = 2;
    goto_xy(2, y++);
    set_color(COLOR_CYAN);
    printf("Resources (%d)", cfg->num_resources);
    y++;

    if (cfg->num_resources == 0) {
        goto_xy(4, y++);
        set_color(COLOR_DIM);
        printf("(No resources defined - press A to add)");
    }

    for (int i = 0; i < cfg->num_resources; i++) {
        goto_xy(4, y);
        if (i == selected) {
            set_color(COLOR_SELECT);
            printf(">");
        } else {
            set_color(COLOR_YELLOW);
            printf(" ");
        }
        printf("[%d] ", i);
        set_color(i == selected ? COLOR_SELECT : COLOR_WHITE);
        printf("%-20s ", cfg->resources[i].name);
        set_color(i == selected ? COLOR_SELECT : COLOR_GREEN);
        printf("count=%-4d avail_at=%d", cfg->resources[i].count, cfg->resources[i].available_at);
        y++;
    }

    goto_xy(2, y + 1);
    set_color(COLOR_DIM);
    printf("A:add  D:delete  Enter/E:edit selected");
}

static void draw_stages_page(DesSimConfig *cfg, int selected) {
    int y = 2;
    goto_xy(2, y++);
    set_color(COLOR_CYAN);
    printf("Stages (%d)", cfg->num_stages);
    y++;

    if (cfg->num_stages == 0) {
        goto_xy(4, y++);
        set_color(COLOR_DIM);
        printf("(No stages defined - press A to add)");
    }

    for (int i = 0; i < cfg->num_stages; i++) {
        goto_xy(4, y);
        if (i == selected) {
            set_color(COLOR_SELECT);
            printf(">");
        } else {
            set_color(COLOR_YELLOW);
            printf(" ");
        }
        printf("[%d] ", i);
        set_color(i == selected ? COLOR_SELECT : COLOR_WHITE);
        printf("%-20s ", cfg->stages[i].name);
        set_color(i == selected ? COLOR_SELECT : COLOR_GREEN);
        const char *resname = (cfg->stages[i].resource_type_id >= 0)
            ? cfg->resources[cfg->stages[i].resource_type_id].name : "-";
        printf("resource=%-16s states=%d events=%d trans=%d outcomes=%d",
               resname,
               cfg->stages[i].num_states,
               cfg->stages[i].num_event_types,
               cfg->stages[i].num_transitions,
               cfg->stages[i].num_outcomes);
        y++;
    }

    goto_xy(2, y + 1);
    set_color(COLOR_DIM);
    printf("A:add  D:delete  Enter/E:edit selected");
}

static void draw_arrivals_page(DesSimConfig *cfg, int selected) {
    int y = 2;
    goto_xy(2, y++);
    set_color(COLOR_CYAN);
    printf("Entity Arrivals (%d)", cfg->num_arrivals);
    y++;

    if (cfg->num_arrivals == 0) {
        goto_xy(4, y++);
        set_color(COLOR_DIM);
        printf("(No arrivals defined - press A to add)");
    }

    for (int i = 0; i < cfg->num_arrivals; i++) {
        goto_xy(4, y);
        if (i == selected) {
            set_color(COLOR_SELECT);
            printf(">");
        } else {
            set_color(COLOR_YELLOW);
            printf(" ");
        }
        printf("[%d] ", i);
        set_color(i == selected ? COLOR_SELECT : COLOR_WHITE);
        printf("%-20s ", cfg->arrivals[i].name);
        set_color(i == selected ? COLOR_SELECT : COLOR_GREEN);
        const char *stagename = (cfg->arrivals[i].entry_stage_id >= 0
            && cfg->arrivals[i].entry_stage_id < cfg->num_stages)
            ? cfg->stages[cfg->arrivals[i].entry_stage_id].name : "?";
        printf("count=%-4d entry=%-16s start=%d pri=%d",
               cfg->arrivals[i].entity_count,
               stagename,
               cfg->arrivals[i].start_time,
               cfg->arrivals[i].priority);
        y++;
    }

    goto_xy(2, y + 1);
    set_color(COLOR_DIM);
    printf("A:add  D:delete  Enter/E:edit selected");
}

static void draw_sim_page(DesSimConfig *cfg) {
    int y = 2;
    goto_xy(2, y++);
    set_color(COLOR_CYAN);
    printf("Simulation Parameters");
    y++;

    goto_xy(4, y++);
    set_color(COLOR_WHITE); printf("Max time:       ");
    set_color(COLOR_GREEN); printf("%d", cfg->max_time);
    goto_xy(4, y++);
    set_color(COLOR_WHITE); printf("Max events:     ");
    set_color(COLOR_GREEN); printf("%d", cfg->max_events);
    goto_xy(4, y++);
    set_color(COLOR_WHITE); printf("Seed:           ");
    set_color(COLOR_GREEN); printf("%u", cfg->seed);
    goto_xy(4, y++);
    set_color(COLOR_WHITE); printf("Record events:  ");
    set_color(COLOR_GREEN); printf("%s", cfg->stats.record_events ? "yes" : "no");
    goto_xy(4, y++);
    set_color(COLOR_WHITE); printf("Record flow:    ");
    set_color(COLOR_GREEN); printf("%s", cfg->stats.record_entity_flow ? "yes" : "no");
    goto_xy(4, y++);
    set_color(COLOR_WHITE); printf("Record util:    ");
    set_color(COLOR_GREEN); printf("%s", cfg->stats.record_resource_util ? "yes" : "no");
    goto_xy(4, y++);
    set_color(COLOR_WHITE); printf("Output dir:     ");
    set_color(COLOR_GREEN); printf("%s", cfg->stats.output_dir);

    goto_xy(2, y + 1);
    set_color(COLOR_DIM);
    printf("E:edit parameters");
}

static void draw_view_page(DesSimConfig *cfg) {
    int y = 2;
    goto_xy(2, y++);
    set_color(COLOR_CYAN);
    printf("View Config Summary");
    y++;

    goto_xy(4, y++);
    set_color(COLOR_WHITE); printf("Resources: %d", cfg->num_resources);
    goto_xy(4, y++);
    set_color(COLOR_WHITE); printf("Stages:    %d", cfg->num_stages);
    goto_xy(4, y++);
    set_color(COLOR_WHITE); printf("Arrivals:  %d", cfg->num_arrivals);
    y++;

    for (int i = 0; i < cfg->num_stages; i++) {
        goto_xy(4, y++);
        set_color(COLOR_YELLOW); printf("%-20s", cfg->stages[i].name);
        set_color(COLOR_DIM); printf(" -> ");
        for (int j = 0; j < cfg->stages[i].num_outcomes; j++) {
            set_color(COLOR_GREEN);
            if (cfg->stages[i].outcomes[j].next_stage_id >= 0) {
                printf("%s(%.0f%%) ",
                       cfg->stages[i].outcomes[j].name,
                       cfg->stages[i].outcomes[j].probability * 100);
            } else {
                printf("%s(%.0f%%)[EXIT] ",
                       cfg->stages[i].outcomes[j].name,
                       cfg->stages[i].outcomes[j].probability * 100);
            }
        }
        if (y >= screenH - 5) break;
    }

    goto_xy(2, y + 1);
    set_color(COLOR_DIM);
    printf("Ctrl+S:save JSON  Ctrl+O:load JSON");
}

/* --- Add dialogs --- */

static int add_resource_dialog(DesSimConfig *cfg) {
    char name[MAX_INPUT] = "";
    char count_str[MAX_INPUT] = "1";
    if (!prompt_line(4, 10, "Resource name:", name, MAX_INPUT, COLOR_CYAN)) return 0;
    if (!prompt_line(4, 12, "Instance count:", count_str, MAX_INPUT, COLOR_CYAN)) return 0;
    int count = atoi(count_str);
    if (count < 1) count = 1;
    int id = DesConfig_addResource(cfg, name, count);
    if (id == DES_INVALID_ID) {
        goto_xy(4, 14);
        set_color(COLOR_RED);
        printf("Error: %s", DesConfig_getLastError(cfg));
        Sleep(1500);
    }
    return 1;
}

static int add_stage_dialog(DesSimConfig *cfg) {
    char name[MAX_INPUT] = "";
    if (!prompt_line(4, 10, "Stage name:", name, MAX_INPUT, COLOR_CYAN)) return 0;
    int stage_id = DesConfig_addStage(cfg, name);
    if (stage_id == DES_INVALID_ID) return 0;

    char buf[MAX_INPUT] = "";
    char prompt[64];

    if (cfg->num_resources > 0) {
        goto_xy(4, 11);
        set_color(COLOR_DIM);
        printf("Resources: ");
        for (int r = 0; r < cfg->num_resources; r++) {
            printf("[%d]%s ", r, cfg->resources[r].name);
        }
    }
    char res_str[MAX_INPUT] = "";
    if (prompt_line(4, 12, "Resource (name or #):", res_str, MAX_INPUT, COLOR_CYAN)) {
        int res_id = -1;
        if (res_str[0] >= '0' && res_str[0] <= '9') {
            res_id = atoi(res_str);
        } else {
            for (int r = 0; r < cfg->num_resources; r++) {
                if (strcmp(cfg->resources[r].name, res_str) == 0) { res_id = r; break; }
            }
        }
        if (res_id >= 0 && res_id < cfg->num_resources) {
            DesStage_setResource(cfg, stage_id, res_id);
        }
    }

    for (int n = 0; ; n++) {
        snprintf(prompt, sizeof(prompt), "State %d name (empty=done):", n);
        if (!prompt_line(4, 14, prompt, buf, MAX_INPUT, COLOR_CYAN)) break;
        if (buf[0] == '\0') break;
        DesStage_addState(cfg, stage_id, buf);
        buf[0] = '\0';
    }

    for (int n = 0; ; n++) {
        snprintf(prompt, sizeof(prompt), "Event %d name (empty=done):", n);
        if (!prompt_line(4, 16, prompt, buf, MAX_INPUT, COLOR_CYAN)) break;
        if (buf[0] == '\0') break;
        DesStage_addEventType(cfg, stage_id, buf);
        buf[0] = '\0';
    }

    goto_xy(4, 17);
    set_color(COLOR_DIM);
    printf("Distributions: fixed, uniform, exponential, normal");
    char dist_str[MAX_INPUT] = "fixed";
    if (!prompt_line(4, 18, "Distribution:", dist_str, MAX_INPUT, COLOR_CYAN)) return 1;
    char p1_str[MAX_INPUT] = "5";
    if (!prompt_line(4, 19, "Param1:", p1_str, MAX_INPUT, COLOR_CYAN)) return 1;
    char p2_str[MAX_INPUT] = "0";
    if (!prompt_line(4, 20, "Param2:", p2_str, MAX_INPUT, COLOR_CYAN)) return 1;

    DesDistType dt = DES_DIST_FIXED;
    if (strcmp(dist_str, "uniform") == 0) dt = DES_DIST_UNIFORM;
    else if (strcmp(dist_str, "exponential") == 0) dt = DES_DIST_EXPONENTIAL;
    else if (strcmp(dist_str, "normal") == 0) dt = DES_DIST_NORMAL;
    DesStage_setProcessingTime(cfg, stage_id, dt, atof(p1_str), atof(p2_str));

    return 1;
}

static int add_arrival_dialog(DesSimConfig *cfg) {
    char name[MAX_INPUT] = "";
    char count_str[MAX_INPUT] = "100";
    char entry_str[MAX_INPUT] = "";
    char interval_str[MAX_INPUT] = "10";
    if (!prompt_line(4, 10, "Entity name:", name, MAX_INPUT, COLOR_CYAN)) return 0;
    if (!prompt_line(4, 12, "Entity count:", count_str, MAX_INPUT, COLOR_CYAN)) return 0;

    if (cfg->num_stages > 0) {
        goto_xy(4, 13);
        set_color(COLOR_DIM);
        printf("Stages: ");
        for (int s = 0; s < cfg->num_stages; s++) {
            printf("[%d]%s ", s, cfg->stages[s].name);
        }
    }
    if (!prompt_line(4, 14, "Entry stage (name or #):", entry_str, MAX_INPUT, COLOR_CYAN)) return 0;

    int entry = -1;
    if (entry_str[0] >= '0' && entry_str[0] <= '9') {
        entry = atoi(entry_str);
    } else {
        for (int s = 0; s < cfg->num_stages; s++) {
            if (strcmp(cfg->stages[s].name, entry_str) == 0) { entry = s; break; }
        }
    }
    if (entry < 0 || entry >= cfg->num_stages) return 0;

    goto_xy(4, 15);
    set_color(COLOR_DIM);
    printf("Distributions: fixed, uniform, exponential, normal");
    char dist_str[MAX_INPUT] = "fixed";
    if (!prompt_line(4, 16, "Distribution:", dist_str, MAX_INPUT, COLOR_CYAN)) return 0;
    if (!prompt_line(4, 17, "Param1:", interval_str, MAX_INPUT, COLOR_CYAN)) return 0;
    char p2_str[MAX_INPUT] = "0";
    if (!prompt_line(4, 18, "Param2:", p2_str, MAX_INPUT, COLOR_CYAN)) return 0;

    DesDistType dt = DES_DIST_FIXED;
    if (strcmp(dist_str, "uniform") == 0) dt = DES_DIST_UNIFORM;
    else if (strcmp(dist_str, "exponential") == 0) dt = DES_DIST_EXPONENTIAL;
    else if (strcmp(dist_str, "normal") == 0) dt = DES_DIST_NORMAL;

    int a = DesConfig_addArrival(cfg, name, atoi(count_str), cfg->stages[entry].name,
                                  dt, atof(interval_str), atof(p2_str));
    if (a == DES_INVALID_ID) return 0;

    char start_str[MAX_INPUT] = "0";
    if (prompt_line(4, 19, "Start time:", start_str, MAX_INPUT, COLOR_CYAN)) {
        DesConfig_setArrivalStart(cfg, a, atoi(start_str));
    }
    return 1;
}

/* --- Edit dialogs --- */

static void edit_resource_dialog(DesSimConfig *cfg, int idx) {
    if (idx < 0 || idx >= cfg->num_resources) return;
    DesResourceDef *r = &cfg->resources[idx];

    goto_xy(4, 10);
    set_color(COLOR_CYAN);
    printf("Editing Resource [%d]: %s", idx, r->name);

    char name[MAX_INPUT];
    snprintf(name, sizeof(name), "%s", r->name);
    if (prompt_line(4, 12, "Name:", name, MAX_INPUT, COLOR_YELLOW)) {
        if (name[0] != '\0')
            DesConfig_setResourceName(cfg, idx, name);
    }

    char count_str[MAX_INPUT];
    snprintf(count_str, sizeof(count_str), "%d", r->count);
    if (prompt_line(4, 14, "Instance count:", count_str, MAX_INPUT, COLOR_YELLOW)) {
        int c = atoi(count_str);
        if (c >= 1) DesConfig_setResourceCount(cfg, idx, c);
    }

    char avail_str[MAX_INPUT];
    snprintf(avail_str, sizeof(avail_str), "%d", r->available_at);
    if (prompt_line(4, 16, "Available at (0=immediate):", avail_str, MAX_INPUT, COLOR_YELLOW)) {
        DesConfig_setResourceAvailableAt(cfg, idx, atoi(avail_str));
    }
}

static void edit_stage_dialog(DesSimConfig *cfg, int idx) {
    if (idx < 0 || idx >= cfg->num_stages) return;
    DesStageDef *s = &cfg->stages[idx];

    goto_xy(4, 10);
    set_color(COLOR_CYAN);
    printf("Editing Stage [%d]: %s", idx, s->name);

    char name[MAX_INPUT];
    snprintf(name, sizeof(name), "%s", s->name);
    if (prompt_line(4, 12, "Name:", name, MAX_INPUT, COLOR_YELLOW)) {
        if (name[0] != '\0')
            DesStage_setName(cfg, idx, name);
    }

    /* Show current resource */
    const char *resname = (s->resource_type_id >= 0 && s->resource_type_id < cfg->num_resources)
        ? cfg->resources[s->resource_type_id].name : "(none)";
    goto_xy(4, 13);
    set_color(COLOR_DIM);
    printf("Current resource: %s", resname);

    if (cfg->num_resources > 0) {
        goto_xy(4, 14);
        set_color(COLOR_DIM);
        printf("Available: ");
        for (int r = 0; r < cfg->num_resources; r++) {
            printf("[%d]%s ", r, cfg->resources[r].name);
        }
    }
    char res_str[MAX_INPUT] = "";
    snprintf(res_str, sizeof(res_str), "%d", s->resource_type_id >= 0 ? s->resource_type_id : -1);
    if (prompt_line(4, 15, "Resource (name or #):", res_str, MAX_INPUT, COLOR_YELLOW)) {
        if (res_str[0] != '\0') {
            int res_id = -1;
            if (res_str[0] >= '0' && res_str[0] <= '9') {
                res_id = atoi(res_str);
            } else {
                for (int r = 0; r < cfg->num_resources; r++) {
                    if (strcmp(cfg->resources[r].name, res_str) == 0) { res_id = r; break; }
                }
            }
            if (res_id == -1 || (res_id >= 0 && res_id < cfg->num_resources)) {
                DesStage_setResource(cfg, idx, res_id);
            }
        }
    }

    /* Processing time */
    const char *dist_str = "fixed";
    if (s->processing_time.type == DES_DIST_UNIFORM) dist_str = "uniform";
    else if (s->processing_time.type == DES_DIST_EXPONENTIAL) dist_str = "exponential";
    else if (s->processing_time.type == DES_DIST_NORMAL) dist_str = "normal";

    goto_xy(4, 16);
    set_color(COLOR_DIM);
    printf("Current: %s p1=%.4g p2=%.4g", dist_str, s->processing_time.param1, s->processing_time.param2);

    goto_xy(4, 17);
    set_color(COLOR_DIM);
    printf("Distributions: fixed, uniform, exponential, normal");

    char distbuf[MAX_INPUT];
    snprintf(distbuf, sizeof(distbuf), "%s", dist_str);
    if (prompt_line(4, 18, "Distribution:", distbuf, MAX_INPUT, COLOR_YELLOW)) {
        char p1buf[MAX_INPUT], p2buf[MAX_INPUT];
        snprintf(p1buf, sizeof(p1buf), "%.4g", s->processing_time.param1);
        snprintf(p2buf, sizeof(p2buf), "%.4g", s->processing_time.param2);
        if (prompt_line(4, 19, "Param1:", p1buf, MAX_INPUT, COLOR_YELLOW) &&
            prompt_line(4, 20, "Param2:", p2buf, MAX_INPUT, COLOR_YELLOW)) {
            DesDistType dt = DES_DIST_FIXED;
            if (strcmp(distbuf, "uniform") == 0) dt = DES_DIST_UNIFORM;
            else if (strcmp(distbuf, "exponential") == 0) dt = DES_DIST_EXPONENTIAL;
            else if (strcmp(distbuf, "normal") == 0) dt = DES_DIST_NORMAL;
            DesStage_setProcessingTime(cfg, idx, dt, atof(p1buf), atof(p2buf));
        }
    }
}

static void edit_arrival_dialog(DesSimConfig *cfg, int idx) {
    if (idx < 0 || idx >= cfg->num_arrivals) return;
    DesEntityArrival *a = &cfg->arrivals[idx];

    goto_xy(4, 10);
    set_color(COLOR_CYAN);
    printf("Editing Arrival [%d]: %s", idx, a->name);

    char name[MAX_INPUT];
    snprintf(name, sizeof(name), "%s", a->name);
    if (prompt_line(4, 12, "Name:", name, MAX_INPUT, COLOR_YELLOW)) {
        if (name[0] != '\0')
            DesConfig_setArrivalName(cfg, idx, name);
    }

    char count_str[MAX_INPUT];
    snprintf(count_str, sizeof(count_str), "%d", a->entity_count);
    if (prompt_line(4, 14, "Entity count:", count_str, MAX_INPUT, COLOR_YELLOW)) {
        int c = atoi(count_str);
        if (c >= 1) DesConfig_setArrivalCount(cfg, idx, c);
    }

    /* Show current entry stage */
    const char *stagename = (a->entry_stage_id >= 0 && a->entry_stage_id < cfg->num_stages)
        ? cfg->stages[a->entry_stage_id].name : "(none)";
    goto_xy(4, 15);
    set_color(COLOR_DIM);
    printf("Current entry: %s", stagename);

    if (cfg->num_stages > 0) {
        goto_xy(4, 16);
        set_color(COLOR_DIM);
        printf("Stages: ");
        for (int s = 0; s < cfg->num_stages; s++) {
            printf("[%d]%s ", s, cfg->stages[s].name);
        }
    }
    char entry_str[MAX_INPUT] = "";
    if (prompt_line(4, 17, "Entry stage (name or #):", entry_str, MAX_INPUT, COLOR_YELLOW)) {
        if (entry_str[0] != '\0') {
            int entry = -1;
            if (entry_str[0] >= '0' && entry_str[0] <= '9') {
                entry = atoi(entry_str);
            } else {
                for (int s = 0; s < cfg->num_stages; s++) {
                    if (strcmp(cfg->stages[s].name, entry_str) == 0) { entry = s; break; }
                }
            }
            if (entry >= 0 && entry < cfg->num_stages) {
                /* Re-create arrival with new entry stage */
                a->entry_stage_id = entry;
            }
        }
    }

    /* Inter-arrival distribution */
    const char *dist_str = "fixed";
    if (a->inter_arrival.type == DES_DIST_UNIFORM) dist_str = "uniform";
    else if (a->inter_arrival.type == DES_DIST_EXPONENTIAL) dist_str = "exponential";
    else if (a->inter_arrival.type == DES_DIST_NORMAL) dist_str = "normal";

    goto_xy(4, 18);
    set_color(COLOR_DIM);
    printf("Current: %s p1=%.4g p2=%.4g", dist_str, a->inter_arrival.param1, a->inter_arrival.param2);

    goto_xy(4, 19);
    set_color(COLOR_DIM);
    printf("Distributions: fixed, uniform, exponential, normal");

    char distbuf[MAX_INPUT];
    snprintf(distbuf, sizeof(distbuf), "%s", dist_str);
    if (prompt_line(4, 20, "Distribution:", distbuf, MAX_INPUT, COLOR_YELLOW)) {
        char p1buf[MAX_INPUT], p2buf[MAX_INPUT];
        snprintf(p1buf, sizeof(p1buf), "%.4g", a->inter_arrival.param1);
        snprintf(p2buf, sizeof(p2buf), "%.4g", a->inter_arrival.param2);
        if (prompt_line(4, 21, "Param1:", p1buf, MAX_INPUT, COLOR_YELLOW) &&
            prompt_line(4, 22, "Param2:", p2buf, MAX_INPUT, COLOR_YELLOW)) {
            DesDistType dt = DES_DIST_FIXED;
            if (strcmp(distbuf, "uniform") == 0) dt = DES_DIST_UNIFORM;
            else if (strcmp(distbuf, "exponential") == 0) dt = DES_DIST_EXPONENTIAL;
            else if (strcmp(distbuf, "normal") == 0) dt = DES_DIST_NORMAL;
            a->inter_arrival.type = dt;
            a->inter_arrival.param1 = atof(p1buf);
            a->inter_arrival.param2 = atof(p2buf);
        }
    }
}

static void edit_sim_dialog(DesSimConfig *cfg) {
    char buf[MAX_INPUT];

    goto_xy(4, 10);
    set_color(COLOR_CYAN);
    printf("Editing Simulation Parameters");

    snprintf(buf, sizeof(buf), "%d", cfg->max_time);
    if (prompt_line(4, 12, "Max time:", buf, MAX_INPUT, COLOR_YELLOW))
        DesConfig_setMaxTime(cfg, atoi(buf));

    snprintf(buf, sizeof(buf), "%d", cfg->max_events);
    if (prompt_line(4, 14, "Max events:", buf, MAX_INPUT, COLOR_YELLOW))
        DesConfig_setMaxEvents(cfg, atoi(buf));

    snprintf(buf, sizeof(buf), "%u", cfg->seed);
    if (prompt_line(4, 16, "Seed:", buf, MAX_INPUT, COLOR_YELLOW))
        DesConfig_setSeed(cfg, (unsigned int)atoi(buf));

    goto_xy(4, 17);
    set_color(COLOR_DIM);
    printf("Record events (0/1):");
    snprintf(buf, sizeof(buf), "%d", cfg->stats.record_events ? 1 : 0);
    if (prompt_line(4, 18, "Record events:", buf, MAX_INPUT, COLOR_YELLOW))
        cfg->stats.record_events = (atoi(buf) != 0);

    snprintf(buf, sizeof(buf), "%d", cfg->stats.record_entity_flow ? 1 : 0);
    if (prompt_line(4, 20, "Record flow:", buf, MAX_INPUT, COLOR_YELLOW))
        cfg->stats.record_entity_flow = (atoi(buf) != 0);

    snprintf(buf, sizeof(buf), "%d", cfg->stats.record_resource_util ? 1 : 0);
    if (prompt_line(4, 22, "Record util:", buf, MAX_INPUT, COLOR_YELLOW))
        cfg->stats.record_resource_util = (atoi(buf) != 0);

    if (prompt_line(4, 24, "Output dir:", cfg->stats.output_dir, sizeof(cfg->stats.output_dir), COLOR_YELLOW)) {
        /* already written in-place by prompt_line */
    }
}

/* --- Load JSON dialog --- */

static int load_json_dialog(DesSimConfig **cfg) {
    char path[MAX_INPUT] = "configs/";
    goto_xy(4, 10);
    set_color(COLOR_CYAN);
    printf("Available configs:");
    goto_xy(4, 11);
    set_color(COLOR_DIM);
    printf("  coffee_shop.json");
    goto_xy(4, 12);
    set_color(COLOR_DIM);
    printf("  airport_security.json");
    goto_xy(4, 13);
    set_color(COLOR_DIM);
    printf("  release_pipeline.json");
    goto_xy(4, 14);
    set_color(COLOR_DIM);
    printf("  whatif_release.json");
    goto_xy(4, 15);
    set_color(COLOR_DIM);
    printf("  theme_park.json");

    if (!prompt_line(4, 17, "Load path:", path, MAX_INPUT, COLOR_CYAN)) return 0;

    DesSimConfig *new_cfg = DesConfig_loadJson(path);
    if (!new_cfg) {
        goto_xy(4, 19);
        set_color(COLOR_RED);
        printf("Failed to load: %s", DesConfig_getLoadError());
        Sleep(2000);
        return 0;
    }

    DesConfig_destroy(*cfg);
    *cfg = new_cfg;

    goto_xy(4, 19);
    set_color(COLOR_GREEN);
    printf("Loaded: %s", path);
    Sleep(1000);
    return 1;
}

/* --- Confirm dialog --- */

static int confirm_dialog(const char *msg) {
    goto_xy(4, 10);
    set_color(COLOR_YELLOW);
    printf("%s (y/n)?", msg);

    INPUT_RECORD ir;
    DWORD count;
    while (1) {
        while (PeekConsoleInput(hConsole, &ir, 1, &count) && count > 0) {
            ReadConsoleInput(hConsole, &ir, 1, &count);
            if (ir.EventType != KEY_EVENT || !ir.Event.KeyEvent.bKeyDown) continue;
            WORD vk = ir.Event.KeyEvent.wVirtualKeyCode;
            CHAR ch = ir.Event.KeyEvent.uChar.AsciiChar;
            if (ch == 'y' || ch == 'Y' || vk == VK_RETURN) return 1;
            if (ch == 'n' || ch == 'N' || vk == VK_ESCAPE) return 0;
        }
        Sleep(20);
    }
}

/* --- Save dialog --- */

static int save_json_dialog(DesSimConfig *cfg) {
    char path[MAX_INPUT] = "configs/my_config.json";
    if (!prompt_line(4, 10, "Save path:", path, MAX_INPUT, COLOR_CYAN)) return 0;

    FILE *f = fopen(path, "w");
    if (!f) {
        goto_xy(4, 12);
        set_color(COLOR_RED);
        printf("Cannot write to: %s", path);
        Sleep(1500);
        return 0;
    }

    fprintf(f, "{\n");
    fprintf(f, "  \"simulation\": {\n");
    fprintf(f, "    \"max_time\": %d,\n", cfg->max_time);
    fprintf(f, "    \"max_events\": %d,\n", cfg->max_events);
    fprintf(f, "    \"seed\": %u\n", cfg->seed);
    fprintf(f, "  },\n");

    fprintf(f, "  \"resources\": [\n");
    for (int i = 0; i < cfg->num_resources; i++) {
        fprintf(f, "    { \"name\": \"%s\", \"count\": %d", cfg->resources[i].name, cfg->resources[i].count);
        if (cfg->resources[i].available_at > 0)
            fprintf(f, ", \"available_at\": %d", cfg->resources[i].available_at);
        fprintf(f, " }%s\n", i < cfg->num_resources - 1 ? "," : "");
    }
    fprintf(f, "  ],\n");

    fprintf(f, "  \"stages\": [\n");
    for (int i = 0; i < cfg->num_stages; i++) {
        DesStageDef *s = &cfg->stages[i];
        fprintf(f, "    {\n");
        fprintf(f, "      \"name\": \"%s\",\n", s->name);
        if (s->resource_type_id >= 0 && s->resource_type_id < cfg->num_resources)
            fprintf(f, "      \"resource\": \"%s\",\n", cfg->resources[s->resource_type_id].name);

        fprintf(f, "      \"states\": [");
        for (int j = 0; j < s->num_states; j++) {
            fprintf(f, "%s\"%s\"", j > 0 ? ", " : "", s->state_names[j]);
        }
        fprintf(f, "],\n");

        fprintf(f, "      \"event_types\": [");
        for (int j = 0; j < s->num_event_types; j++) {
            fprintf(f, "%s\"%s\"", j > 0 ? ", " : "", s->event_type_names[j]);
        }
        fprintf(f, "],\n");

        const char *ds = "fixed";
        if (s->processing_time.type == DES_DIST_UNIFORM) ds = "uniform";
        else if (s->processing_time.type == DES_DIST_EXPONENTIAL) ds = "exponential";
        else if (s->processing_time.type == DES_DIST_NORMAL) ds = "normal";
        fprintf(f, "      \"processing_time\": { \"distribution\": \"%s\", \"param1\": %.4g, \"param2\": %.4g },\n",
                ds, s->processing_time.param1, s->processing_time.param2);

        fprintf(f, "      \"fsm\": [\n");
        for (int j = 0; j < s->num_transitions; j++) {
            DesFsmTransition *t = &s->transitions[j];
            const char *action = "none";
            switch (t->action_type) {
                case DES_ACTION_ACQUIRE_AND_PROCESS: action = "acquire_and_process"; break;
                case DES_ACTION_RELEASE_AND_DISPATCH: action = "release_and_dispatch"; break;
                case DES_ACTION_RELEASE_AND_RETRY: action = "release_and_retry"; break;
                case DES_ACTION_WAIT_RETRY: action = "wait_retry"; break;
                default: break;
            }
            fprintf(f, "        { \"state\": \"%s\", \"event\": \"%s\", \"next_state\": \"%s\", \"action\": \"%s\" }%s\n",
                    s->state_names[t->state_index], s->event_type_names[t->event_index],
                    s->state_names[t->next_state_index], action,
                    j < s->num_transitions - 1 ? "," : "");
        }
        fprintf(f, "      ],\n");

        fprintf(f, "      \"outcomes\": [\n");
        for (int j = 0; j < s->num_outcomes; j++) {
            DesStageOutcome *o = &s->outcomes[j];
            fprintf(f, "        { \"name\": \"%s\", \"probability\": %.4g", o->name, o->probability);
            if (o->next_stage_id >= 0 && o->next_stage_id < cfg->num_stages)
                fprintf(f, ", \"next_stage\": \"%s\"", cfg->stages[o->next_stage_id].name);
            fprintf(f, " }%s\n", j < s->num_outcomes - 1 ? "," : "");
        }
        fprintf(f, "      ]\n");

        fprintf(f, "    }%s\n", i < cfg->num_stages - 1 ? "," : "");
    }
    fprintf(f, "  ],\n");

    fprintf(f, "  \"entity_arrivals\": [\n");
    for (int i = 0; i < cfg->num_arrivals; i++) {
        DesEntityArrival *a = &cfg->arrivals[i];
        const char *stage_name = (a->entry_stage_id >= 0 && a->entry_stage_id < cfg->num_stages)
            ? cfg->stages[a->entry_stage_id].name : "unknown";
        fprintf(f, "    { \"name\": \"%s\", \"count\": %d, \"entry_stage\": \"%s\"",
                a->name, a->entity_count, stage_name);
        fprintf(f, ", \"inter_arrival\": { \"distribution\": \"fixed\", \"param1\": %.4g }", a->inter_arrival.param1);
        if (a->start_time > 0) fprintf(f, ", \"start_time\": %d", a->start_time);
        if (a->priority > 0) fprintf(f, ", \"priority\": %d", a->priority);
        fprintf(f, " }%s\n", i < cfg->num_arrivals - 1 ? "," : "");
    }
    fprintf(f, "  ],\n");

    fprintf(f, "  \"statistics\": {\n");
    fprintf(f, "    \"record_events\": %s,\n", cfg->stats.record_events ? "true" : "false");
    fprintf(f, "    \"record_entity_flow\": %s,\n", cfg->stats.record_entity_flow ? "true" : "false");
    fprintf(f, "    \"record_resource_util\": %s,\n", cfg->stats.record_resource_util ? "true" : "false");
    fprintf(f, "    \"output_dir\": \"%s\"\n", cfg->stats.output_dir);
    fprintf(f, "  }\n");
    fprintf(f, "}\n");

    fclose(f);

    goto_xy(4, 12);
    set_color(COLOR_GREEN);
    printf("Saved to %s", path);
    Sleep(1000);
    return 1;
}

int main(int argc, char *argv[]) {
    DesSimConfig *cfg = NULL;

    if (argc > 1) {
        cfg = DesConfig_loadJson(argv[1]);
        if (!cfg) {
            fprintf(stderr, "Failed to load %s: %s\n", argv[1], DesConfig_getLoadError());
        }
    }
    if (!cfg) {
        cfg = DesConfig_create();
    }

    console_init();

    Page page = PAGE_RESOURCES;
    int selected = 0;
    int running = 1;

    while (running) {
        clear_screen();
        draw_tabs(page);

        switch (page) {
            case PAGE_RESOURCES: draw_resources_page(cfg, selected); break;
            case PAGE_STAGES:    draw_stages_page(cfg, selected); break;
            case PAGE_ARRIVALS:  draw_arrivals_page(cfg, selected); break;
            case PAGE_SIM:       draw_sim_page(cfg); break;
            case PAGE_VIEW:      draw_view_page(cfg); break;
            default: break;
        }

        draw_footer();

        INPUT_RECORD ir;
        DWORD count;
        while (1) {
            Sleep(20);
            while (PeekConsoleInput(hConsole, &ir, 1, &count) && count > 0) {
                ReadConsoleInput(hConsole, &ir, 1, &count);
                if (ir.EventType != KEY_EVENT || !ir.Event.KeyEvent.bKeyDown) continue;

                WORD vk = ir.Event.KeyEvent.wVirtualKeyCode;
                DWORD ctrl = ir.Event.KeyEvent.dwControlKeyState;
                CHAR ch = ir.Event.KeyEvent.uChar.AsciiChar;

                int max_items = 0;
                switch (page) {
                    case PAGE_RESOURCES: max_items = cfg->num_resources; break;
                    case PAGE_STAGES:    max_items = cfg->num_stages; break;
                    case PAGE_ARRIVALS:  max_items = cfg->num_arrivals; break;
                    default: break;
                }

                if (vk == 'Q' || vk == VK_ESCAPE) {
                    running = 0;
                    goto done;

                } else if (vk == VK_TAB) {
                    page = (Page)((page + 1) % PAGE_COUNT);
                    selected = 0;
                    goto redraw;

                } else if (vk == VK_UP) {
                    if (selected > 0) selected--;
                    goto redraw;

                } else if (vk == VK_DOWN) {
                    if (selected < max_items - 1) selected++;
                    goto redraw;

                } else if (vk == 'A' || vk == 'a') {
                    switch (page) {
                        case PAGE_RESOURCES: add_resource_dialog(cfg); break;
                        case PAGE_STAGES:    add_stage_dialog(cfg); break;
                        case PAGE_ARRIVALS:  add_arrival_dialog(cfg); break;
                        default: break;
                    }
                    goto redraw;

                } else if (vk == VK_RETURN || ch == 'e' || ch == 'E') {
                    switch (page) {
                        case PAGE_RESOURCES:
                            if (selected >= 0 && selected < cfg->num_resources)
                                edit_resource_dialog(cfg, selected);
                            break;
                        case PAGE_STAGES:
                            if (selected >= 0 && selected < cfg->num_stages)
                                edit_stage_dialog(cfg, selected);
                            break;
                        case PAGE_ARRIVALS:
                            if (selected >= 0 && selected < cfg->num_arrivals)
                                edit_arrival_dialog(cfg, selected);
                            break;
                        case PAGE_SIM:
                            edit_sim_dialog(cfg);
                            break;
                        default: break;
                    }
                    goto redraw;

                } else if (ch == 'd' || ch == 'D') {
                    if (page == PAGE_RESOURCES && selected >= 0 && selected < cfg->num_resources) {
                        if (confirm_dialog("Delete resource")) {
                            DesConfig_removeResource(cfg, selected);
                            if (selected >= cfg->num_resources && selected > 0)
                                selected = cfg->num_resources - 1;
                        }
                    } else if (page == PAGE_STAGES && selected >= 0 && selected < cfg->num_stages) {
                        if (confirm_dialog("Delete stage")) {
                            DesConfig_removeStage(cfg, selected);
                            if (selected >= cfg->num_stages && selected > 0)
                                selected = cfg->num_stages - 1;
                        }
                    } else if (page == PAGE_ARRIVALS && selected >= 0 && selected < cfg->num_arrivals) {
                        if (confirm_dialog("Delete arrival")) {
                            DesConfig_removeArrival(cfg, selected);
                            if (selected >= cfg->num_arrivals && selected > 0)
                                selected = cfg->num_arrivals - 1;
                        }
                    }
                    goto redraw;

                } else if (vk == 'S' && (ctrl & (DWORD)LEFT_CTRL_PRESSED)) {
                    save_json_dialog(cfg);
                    goto redraw;

                } else if (vk == 'O' && (ctrl & (DWORD)LEFT_CTRL_PRESSED)) {
                    load_json_dialog(&cfg);
                    selected = 0;
                    goto redraw;
                }
            }
        }
        redraw:
        continue;
        done:
        break;
    }

    DesConfig_destroy(cfg);
    return 0;
}
