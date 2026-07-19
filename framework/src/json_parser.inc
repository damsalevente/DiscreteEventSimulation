static char *json_strdup(const char *s, int len) {
    char *out = (char *)malloc((size_t)len + 1);
    if (out) { memcpy(out, s, (size_t)len); out[len] = '\0'; }
    return out;
}

static cJSON *json_newnode(int type) {
    cJSON *n = (cJSON *)calloc(1, sizeof(cJSON));
    n->type = type;
    return n;
}

static void cJSON_Delete(cJSON *c) {
    if (!c) return;
    cJSON_Delete(c->child);
    cJSON_Delete(c->next);
    free(c->string);
    free(c->valuestring);
    free(c);
}

static cJSON *cJSON_GetObjectItem(const cJSON *object, const char *string) {
    if (!object || !string) return NULL;
    cJSON *child = object->child;
    while (child) {
        if (child->string && strcmp(child->string, string) == 0) return child;
        child = child->next;
    }
    return NULL;
}

static cJSON *cJSON_GetArrayItem(const cJSON *array, int index) {
    if (!array) return NULL;
    cJSON *c = array->child;
    for (int i = 0; i < index && c; i++) c = c->next;
    return c;
}

static int cJSON_GetArraySize(const cJSON *array) {
    if (!array) return 0;
    int n = 0;
    cJSON *c = array->child;
    while (c) { n++; c = c->next; }
    return n;
}

static double cJSON_GetNumberValue(const cJSON *item) {
    if (!item) return 0;
    return item->valuedouble;
}

static const char *cJSON_GetStringValue(const cJSON *item) {
    if (!item) return NULL;
    return item->valuestring;
}

typedef struct {
    const char *ptr;
    int pos;
} JsonParser;

static void skipWhitespace(JsonParser *p) {
    while (p->ptr[p->pos] == ' ' || p->ptr[p->pos] == '\t' ||
           p->ptr[p->pos] == '\n' || p->ptr[p->pos] == '\r') p->pos++;
}

static char peek(JsonParser *p) { return p->ptr[p->pos]; }
static char advance(JsonParser *p) { return p->ptr[p->pos++]; }

static cJSON *parseValue(JsonParser *p);
static cJSON *parseObject(JsonParser *p);
static cJSON *parseArray(JsonParser *p);
static cJSON *parseString(JsonParser *p);
static cJSON *parseNumber(JsonParser *p);

static cJSON *parseString(JsonParser *p) {
    skipWhitespace(p);
    if (peek(p) != '"') return NULL;
    advance(p);
    int start = p->pos;
    while (p->ptr[p->pos] && p->ptr[p->pos] != '"') {
        if (p->ptr[p->pos] == '\\') p->pos++;
        p->pos++;
    }
    int len = p->pos - start;
    if (p->ptr[p->pos] == '"') p->pos++;
    cJSON *n = json_newnode(cJSON_String);
    n->valuestring = json_strdup(p->ptr + start, len);
    n->stringlen = len;
    return n;
}

static cJSON *parseNumber(JsonParser *p) {
    skipWhitespace(p);
    int start = p->pos;
    if (p->ptr[p->pos] == '-') p->pos++;
    while (p->ptr[p->pos] >= '0' && p->ptr[p->pos] <= '9') p->pos++;
    if (p->ptr[p->pos] == '.') {
        p->pos++;
        while (p->ptr[p->pos] >= '0' && p->ptr[p->pos] <= '9') p->pos++;
    }
    int len = p->pos - start;
    cJSON *n = json_newnode(cJSON_Number);
    char *tmp = json_strdup(p->ptr + start, len);
    n->valuedouble = atof(tmp);
    n->valueint = (int)n->valuedouble;
    free(tmp);
    return n;
}

static cJSON *parseArray(JsonParser *p) {
    skipWhitespace(p);
    if (peek(p) != '[') return NULL;
    advance(p);
    cJSON *arr = json_newnode(cJSON_Array);
    cJSON *tail = NULL;
    skipWhitespace(p);
    while (peek(p) != ']' && peek(p) != '\0') {
        cJSON *item = parseValue(p);
        if (!item) { cJSON_Delete(arr); return NULL; }
        item->prev = tail;
        if (tail) tail->next = item;
        else arr->child = item;
        tail = item;
        skipWhitespace(p);
        if (peek(p) == ',') { advance(p); skipWhitespace(p); }
    }
    if (peek(p) == ']') advance(p);
    return arr;
}

static cJSON *parseObject(JsonParser *p) {
    skipWhitespace(p);
    if (peek(p) != '{') return NULL;
    advance(p);
    cJSON *obj = json_newnode(cJSON_Object);
    cJSON *tail = NULL;
    skipWhitespace(p);
    while (peek(p) != '}' && peek(p) != '\0') {
        cJSON *key = parseString(p);
        if (!key) { cJSON_Delete(obj); return NULL; }
        skipWhitespace(p);
        if (peek(p) == ':') advance(p);
        cJSON *val = parseValue(p);
        if (!val) { cJSON_Delete(key); cJSON_Delete(obj); return NULL; }
        val->string = key->valuestring;
        key->valuestring = NULL;
        cJSON_Delete(key);
        val->prev = tail;
        if (tail) tail->next = val;
        else obj->child = val;
        tail = val;
        skipWhitespace(p);
        if (peek(p) == ',') { advance(p); skipWhitespace(p); }
    }
    if (peek(p) == '}') advance(p);
    return obj;
}

static cJSON *parseValue(JsonParser *p) {
    skipWhitespace(p);
    char c = peek(p);
    if (c == '"') return parseString(p);
    if (c == '{') return parseObject(p);
    if (c == '[') return parseArray(p);
    if (c == 't' || c == 'f' || c == 'n') {
        int start = p->pos;
        while (p->ptr[p->pos] && p->ptr[p->pos] != ',' &&
               p->ptr[p->pos] != '}' && p->ptr[p->pos] != ']') p->pos++;
        int len = p->pos - start;
        char *tmp = json_strdup(p->ptr + start, len);
        cJSON *n;
        if (strcmp(tmp, "true") == 0) { n = json_newnode(cJSON_True); n->valuedouble = 1; }
        else if (strcmp(tmp, "false") == 0) { n = json_newnode(cJSON_False); n->valuedouble = 0; }
        else { n = json_newnode(cJSON_NULL); }
        free(tmp);
        return n;
    }
    if (c == '-' || (c >= '0' && c <= '9')) return parseNumber(p);
    return NULL;
}

static cJSON *cJSON_Parse(const char *value) {
    JsonParser p;
    p.ptr = value;
    p.pos = 0;
    return parseValue(&p);
}
