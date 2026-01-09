/*
 * Copyright (C) 2026 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "gtest/gtest.h"

#include "interpreter.h"

#include "berberis/base/int.h"

namespace berberis {

namespace {

static_assert(Interpreter::NumberOfRegistersInvolved(
                  Interpreter::VectorRegisterGroupMultiplier::kEigthOfRegister) == 1);
static_assert(Interpreter::NumberOfRegistersInvolved(
                  Interpreter::VectorRegisterGroupMultiplier::kQuarterOfRegister) == 1);
static_assert(Interpreter::NumberOfRegistersInvolved(
                  Interpreter::VectorRegisterGroupMultiplier::kHalfOfRegister) == 1);
static_assert(Interpreter::NumberOfRegistersInvolved(
                  Interpreter::VectorRegisterGroupMultiplier::k1register) == 1);
static_assert(Interpreter::NumberOfRegistersInvolved(
                  Interpreter::VectorRegisterGroupMultiplier::k2registers) == 2);
static_assert(Interpreter::NumberOfRegistersInvolved(
                  Interpreter::VectorRegisterGroupMultiplier::k4registers) == 4);
static_assert(Interpreter::NumberOfRegistersInvolved(
                  Interpreter::VectorRegisterGroupMultiplier::k8registers) == 8);

static_assert(Interpreter::NumRegistersInvolvedForWideOperand(
                  Interpreter::VectorRegisterGroupMultiplier::kEigthOfRegister) == 1);
static_assert(Interpreter::NumRegistersInvolvedForWideOperand(
                  Interpreter::VectorRegisterGroupMultiplier::kQuarterOfRegister) == 1);
static_assert(Interpreter::NumRegistersInvolvedForWideOperand(
                  Interpreter::VectorRegisterGroupMultiplier::kHalfOfRegister) == 1);
static_assert(Interpreter::NumRegistersInvolvedForWideOperand(
                  Interpreter::VectorRegisterGroupMultiplier::k1register) == 2);
static_assert(Interpreter::NumRegistersInvolvedForWideOperand(
                  Interpreter::VectorRegisterGroupMultiplier::k2registers) == 4);
static_assert(Interpreter::NumRegistersInvolvedForWideOperand(
                  Interpreter::VectorRegisterGroupMultiplier::k4registers) == 8);

static_assert(Interpreter::GetVlmax(Interpreter::kType<Int8>,
                                    Interpreter::VectorRegisterGroupMultiplier::kEigthOfRegister) ==
              2);
static_assert(
    Interpreter::GetVlmax(Interpreter::kType<Int8>,
                          Interpreter::VectorRegisterGroupMultiplier::kQuarterOfRegister) == 4);
static_assert(Interpreter::GetVlmax(Interpreter::kType<Int8>,
                                    Interpreter::VectorRegisterGroupMultiplier::kHalfOfRegister) ==
              8);
static_assert(Interpreter::GetVlmax(Interpreter::kType<Int8>,
                                    Interpreter::VectorRegisterGroupMultiplier::k1register) == 16);
static_assert(Interpreter::GetVlmax(Interpreter::kType<Int8>,
                                    Interpreter::VectorRegisterGroupMultiplier::k2registers) == 32);
static_assert(Interpreter::GetVlmax(Interpreter::kType<Int8>,
                                    Interpreter::VectorRegisterGroupMultiplier::k4registers) == 64);
static_assert(Interpreter::GetVlmax(Interpreter::kType<Int8>,
                                    Interpreter::VectorRegisterGroupMultiplier::k8registers) ==
              128);

static_assert(Interpreter::GetVlmax(Interpreter::kType<Int16>,
                                    Interpreter::VectorRegisterGroupMultiplier::kEigthOfRegister) ==
              1);
static_assert(
    Interpreter::GetVlmax(Interpreter::kType<Int16>,
                          Interpreter::VectorRegisterGroupMultiplier::kQuarterOfRegister) == 2);
static_assert(Interpreter::GetVlmax(Interpreter::kType<Int16>,
                                    Interpreter::VectorRegisterGroupMultiplier::kHalfOfRegister) ==
              4);
static_assert(Interpreter::GetVlmax(Interpreter::kType<Int16>,
                                    Interpreter::VectorRegisterGroupMultiplier::k1register) == 8);
static_assert(Interpreter::GetVlmax(Interpreter::kType<Int16>,
                                    Interpreter::VectorRegisterGroupMultiplier::k2registers) == 16);
static_assert(Interpreter::GetVlmax(Interpreter::kType<Int16>,
                                    Interpreter::VectorRegisterGroupMultiplier::k4registers) == 32);
static_assert(Interpreter::GetVlmax(Interpreter::kType<Int16>,
                                    Interpreter::VectorRegisterGroupMultiplier::k8registers) == 64);

static_assert(
    Interpreter::GetVlmax(Interpreter::kType<Int32>,
                          Interpreter::VectorRegisterGroupMultiplier::kQuarterOfRegister) == 1);
static_assert(Interpreter::GetVlmax(Interpreter::kType<Int32>,
                                    Interpreter::VectorRegisterGroupMultiplier::kHalfOfRegister) ==
              2);
static_assert(Interpreter::GetVlmax(Interpreter::kType<Int32>,
                                    Interpreter::VectorRegisterGroupMultiplier::k1register) == 4);
static_assert(Interpreter::GetVlmax(Interpreter::kType<Int32>,
                                    Interpreter::VectorRegisterGroupMultiplier::k2registers) == 8);
static_assert(Interpreter::GetVlmax(Interpreter::kType<Int32>,
                                    Interpreter::VectorRegisterGroupMultiplier::k4registers) == 16);
static_assert(Interpreter::GetVlmax(Interpreter::kType<Int32>,
                                    Interpreter::VectorRegisterGroupMultiplier::k8registers) == 32);

static_assert(Interpreter::GetVlmax(Interpreter::kType<Int64>,
                                    Interpreter::VectorRegisterGroupMultiplier::kHalfOfRegister) ==
              1);
static_assert(Interpreter::GetVlmax(Interpreter::kType<Int64>,
                                    Interpreter::VectorRegisterGroupMultiplier::k1register) == 2);
static_assert(Interpreter::GetVlmax(Interpreter::kType<Int64>,
                                    Interpreter::VectorRegisterGroupMultiplier::k2registers) == 4);
static_assert(Interpreter::GetVlmax(Interpreter::kType<Int64>,
                                    Interpreter::VectorRegisterGroupMultiplier::k4registers) == 8);
static_assert(Interpreter::GetVlmax(Interpreter::kType<Int64>,
                                    Interpreter::VectorRegisterGroupMultiplier::k8registers) == 16);

}  // namespace

}  // namespace berberis
