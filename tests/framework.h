#pragma once
// ============================================================
// Framework de testes leve — sem dependências externas
// ============================================================
#include <stdio.h>
#include <math.h>
#include <string.h>

static int _pass  = 0;
static int _fail  = 0;
static int _current_failed = 0;

// ---- Asserções ----

#define ASSERT(cond) \
    do { if (!(cond)) { \
        printf("    FALHA  %s:%d  condição: %s\n", __FILE__, __LINE__, #cond); \
        _current_failed++; _fail++; \
    } } while(0)

#define ASSERT_EQ(expected, actual) \
    do { if ((expected) != (actual)) { \
        printf("    FALHA  %s:%d  esperado=%lld  obtido=%lld\n", \
               __FILE__, __LINE__, (long long)(expected), (long long)(actual)); \
        _current_failed++; _fail++; \
    } } while(0)

#define ASSERT_FLOAT_EQ(expected, actual, eps) \
    do { if (fabs((double)(expected) - (double)(actual)) > (double)(eps)) { \
        printf("    FALHA  %s:%d  esperado=%.6f  obtido=%.6f\n", \
               __FILE__, __LINE__, (double)(expected), (double)(actual)); \
        _current_failed++; _fail++; \
    } } while(0)

#define ASSERT_STR_EQ(expected, actual) \
    do { if (strcmp((expected),(actual)) != 0) { \
        printf("    FALHA  %s:%d  esperado=\"%s\"  obtido=\"%s\"\n", \
               __FILE__, __LINE__, (expected), (actual)); \
        _current_failed++; _fail++; \
    } } while(0)

#define ASSERT_STR_CONTAINS(haystack, needle) \
    do { if (strstr((haystack),(needle)) == NULL) { \
        printf("    FALHA  %s:%d  \"%s\" não contém \"%s\"\n", \
               __FILE__, __LINE__, (haystack), (needle)); \
        _current_failed++; _fail++; \
    } } while(0)

#define ASSERT_TRUE(x)  ASSERT(x)
#define ASSERT_FALSE(x) ASSERT(!(x))

// ---- Executor de teste individual ----

#define RUN_TEST(fn) \
    do { \
        _current_failed = 0; \
        printf("  %-55s", #fn); \
        fn(); \
        if (_current_failed == 0) { printf("OK\n"); _pass++; } \
    } while(0)

// ---- Cabeçalho de suite ----
#define TEST_SUITE(name) \
    printf("\n=== %s ===\n", name)

// ---- Resumo final — usado em main() ----
#define TEST_SUMMARY() \
    do { \
        printf("\n--------------------------------------------------\n"); \
        printf("Resultado: %d passaram, %d falharam\n", _pass, _fail); \
        return (_fail > 0) ? 1 : 0; \
    } while(0)
