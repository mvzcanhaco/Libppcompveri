#pragma once
// include/ppcomp_version.h
// Constantes de versão da libppcomp.
// Este header é seguro para incluir em builds Android e de teste.

// ─── Versão da biblioteca ─────────────────────────────────────────────────────

#define PPCOMP_VERSION_MAJOR  1
#define PPCOMP_VERSION_MINOR  0
#define PPCOMP_VERSION_PATCH  0
#define PPCOMP_VERSION_STRING "1.0.0"

// ─── Versão do protocolo ABECS ────────────────────────────────────────────────

#define PPCOMP_ABECS_SPEC     "2.20"
#define PPCOMP_ADK_VERSION    "5.0.3"

// ─── Identificação do terminal suportado ─────────────────────────────────────

#define PPCOMP_TERMINAL_MODEL "V660P-A"
#define PPCOMP_TERMINAL_ABI   "arm64-v8a"

// ─── Build info ───────────────────────────────────────────────────────────────

// PPCOMP_BUILD_TYPE é definido pelo CMake como "Release" ou "Debug"
#ifndef PPCOMP_BUILD_TYPE
#define PPCOMP_BUILD_TYPE "Unknown"
#endif
