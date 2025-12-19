/*
 * Copyright (C) 2025 The Android Open Source Project
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

jvmtiHeapCallbacks WrapGuestJvmtiHeapCallbacks(
    GuestType<const jvmtiHeapCallbacks*> guest_callbacks) {
  jvmtiHeapCallbacks host_jvmti_heap_callbacks{};
  host_jvmti_heap_callbacks.heap_iteration_callback =
      WrapGuestFunction(GuestType{guest_callbacks->heap_iteration_callback},
                        "jvmtiHeapCallbacks-heap_iteration_callback");
  host_jvmti_heap_callbacks.heap_reference_callback =
      WrapGuestFunction(GuestType{guest_callbacks->heap_reference_callback},
                        "jvmtiHeapCallbacks-heap_reference_callback");
  host_jvmti_heap_callbacks.primitive_field_callback =
      WrapGuestFunction(GuestType{guest_callbacks->primitive_field_callback},
                        "jvmtiHeapCallbacks-primitive_field_callback");
  host_jvmti_heap_callbacks.array_primitive_value_callback =
      WrapGuestFunction(GuestType{guest_callbacks->array_primitive_value_callback},
                        "jvmtiHeapCallbacks-array_primitive_value_callback");
  host_jvmti_heap_callbacks.string_primitive_value_callback =
      WrapGuestFunction(GuestType{guest_callbacks->string_primitive_value_callback},
                        "jvmtiHeapCallbacks-string_primitive_value_callback");

  return host_jvmti_heap_callbacks;
}

jvmtiEventCallbacks WrapGuestJvmtiEventCallbacks(
    GuestType<const jvmtiEventCallbacks*> guest_callbacks) {
  jvmtiEventCallbacks host_callbacks{};

  host_callbacks.VMInit =
      WrapGuestFunction(GuestType{guest_callbacks->VMInit}, "jvmtiEventCallbacks-VMInit");
  host_callbacks.VMDeath =
      WrapGuestFunction(GuestType{guest_callbacks->VMDeath}, "jvmtiEventCallbacks-VMDeath");
  host_callbacks.ThreadStart =
      WrapGuestFunction(GuestType{guest_callbacks->ThreadStart}, "jvmtiEventCallbacks-ThreadStart");
  host_callbacks.ThreadEnd =
      WrapGuestFunction(GuestType{guest_callbacks->ThreadEnd}, "jvmtiEventCallbacks-ThreadEnd");
  host_callbacks.ClassFileLoadHook = WrapGuestFunction(
      GuestType{guest_callbacks->ClassFileLoadHook}, "jvmtiEventCallbacks-ClassFileLoadHook");
  host_callbacks.ClassLoad =
      WrapGuestFunction(GuestType{guest_callbacks->ClassLoad}, "jvmtiEventCallbacks-ClassLoad");
  host_callbacks.ClassPrepare = WrapGuestFunction(GuestType{guest_callbacks->ClassPrepare},
                                                  "jvmtiEventCallbacks-ClassPrepare");
  host_callbacks.VMStart =
      WrapGuestFunction(GuestType{guest_callbacks->VMStart}, "jvmtiEventCallbacks-VMStart");
  host_callbacks.Exception =
      WrapGuestFunction(GuestType{guest_callbacks->Exception}, "jvmtiEventCallbacks-Exception");
  host_callbacks.ExceptionCatch = WrapGuestFunction(GuestType{guest_callbacks->ExceptionCatch},
                                                    "jvmtiEventCallbacks-ExceptionCatch");
  host_callbacks.SingleStep =
      WrapGuestFunction(GuestType{guest_callbacks->SingleStep}, "jvmtiEventCallbacks-SingleStep");
  host_callbacks.FramePop =
      WrapGuestFunction(GuestType{guest_callbacks->FramePop}, "jvmtiEventCallbacks-FramePop");
  host_callbacks.Breakpoint =
      WrapGuestFunction(GuestType{guest_callbacks->Breakpoint}, "jvmtiEventCallbacks-Breakpoint");
  host_callbacks.FieldAccess =
      WrapGuestFunction(GuestType{guest_callbacks->FieldAccess}, "jvmtiEventCallbacks-FieldAccess");
  host_callbacks.FieldModification = WrapGuestFunction(
      GuestType{guest_callbacks->FieldModification}, "jvmtiEventCallbacks-FieldModification");
  host_callbacks.MethodEntry =
      WrapGuestFunction(GuestType{guest_callbacks->MethodEntry}, "jvmtiEventCallbacks-MethodEntry");
  host_callbacks.MethodExit =
      WrapGuestFunction(GuestType{guest_callbacks->MethodExit}, "jvmtiEventCallbacks-MethodExit");
  host_callbacks.NativeMethodBind = WrapGuestFunction(GuestType{guest_callbacks->NativeMethodBind},
                                                      "jvmtiEventCallbacks-NativeMethodBind");
  host_callbacks.CompiledMethodLoad = WrapGuestFunction(
      GuestType{guest_callbacks->CompiledMethodLoad}, "jvmtiEventCallbacks-CompiledMethodLoad");
  host_callbacks.CompiledMethodUnload = WrapGuestFunction(
      GuestType{guest_callbacks->CompiledMethodUnload}, "jvmtiEventCallbacks-CompiledMethodUnload");
  host_callbacks.DynamicCodeGenerated = WrapGuestFunction(
      GuestType{guest_callbacks->DynamicCodeGenerated}, "jvmtiEventCallbacks-DynamicCodeGenerated");
  host_callbacks.DataDumpRequest = WrapGuestFunction(GuestType{guest_callbacks->DataDumpRequest},
                                                     "jvmtiEventCallbacks-DataDumpRequest");
  host_callbacks.MonitorWait =
      WrapGuestFunction(GuestType{guest_callbacks->MonitorWait}, "jvmtiEventCallbacks-MonitorWait");
  host_callbacks.MonitorWaited = WrapGuestFunction(GuestType{guest_callbacks->MonitorWaited},
                                                   "jvmtiEventCallbacks-MonitorWaited");
  host_callbacks.MonitorContendedEnter =
      WrapGuestFunction(GuestType{guest_callbacks->MonitorContendedEnter},
                        "jvmtiEventCallbacks-MonitorContendedEnter");
  host_callbacks.MonitorContendedEntered =
      WrapGuestFunction(GuestType{guest_callbacks->MonitorContendedEntered},
                        "jvmtiEventCallbacks-MonitorContendedEntered");
  host_callbacks.ResourceExhausted = WrapGuestFunction(
      GuestType{guest_callbacks->ResourceExhausted}, "jvmtiEventCallbacks-ResourceExhausted");
  host_callbacks.GarbageCollectionStart =
      WrapGuestFunction(GuestType{guest_callbacks->GarbageCollectionStart},
                        "jvmtiEventCallbacks-GarbageCollectionStart");
  host_callbacks.GarbageCollectionFinish =
      WrapGuestFunction(GuestType{guest_callbacks->GarbageCollectionFinish},
                        "jvmtiEventCallbacks-GarbageCollectionFinish");
  host_callbacks.ObjectFree =
      WrapGuestFunction(GuestType{guest_callbacks->ObjectFree}, "jvmtiEventCallbacks-ObjectFree");
  host_callbacks.VMObjectAlloc = WrapGuestFunction(GuestType{guest_callbacks->VMObjectAlloc},
                                                   "jvmtiEventCallbacks-VMObjectAlloc");

  return host_callbacks;
}

void DoTrampoline_jvmtiEnv_RunAgentThread(HostCode /* callee */, ProcessState* state) {
  LOG_JNI("DoTrampoline_jvmtiEnv_RunAgentThread called");
  using PFN_callee = decltype(std::declval<jvmtiEnv>().functions->RunAgentThread);
  auto [arg_env, arg_1, arg_2, arg_3, arg_4] = GuestParamsValues<PFN_callee>(state);
  jvmtiEnv* arg_0 = ToHostJvmtiEnv(arg_env);
  auto&& [ret] = GuestReturnReference<PFN_callee>(state);

  auto arg_2_host_fn_ptr = WrapGuestFunction(arg_2, "RunAgentThread-jvmtiStartFunction");
  ret = arg_0->functions->RunAgentThread(arg_0, arg_1, arg_2_host_fn_ptr, arg_3, arg_4);
}

void DoTrampoline_jvmtiEnv_IterateOverObjectsReachableFromObject(HostCode /* callee */,
                                                                 ProcessState* state) {
  LOG_JNI("DoTrampoline_jvmtiEnv_IterateOverObjectsReachableFromObject called");
  using PFN_callee =
      decltype(std::declval<jvmtiEnv>().functions->IterateOverObjectsReachableFromObject);
  auto [arg_env, arg_1, arg_2, arg_3] = GuestParamsValues<PFN_callee>(state);
  jvmtiEnv* arg_0 = ToHostJvmtiEnv(arg_env);
  auto&& [ret] = GuestReturnReference<PFN_callee>(state);

  auto arg_2_host_fn_ptr = WrapGuestFunction(
      arg_2, "IterateOverObjectsReachableFromObject-jvmtiObjectReferenceCallback");
  ret = arg_0->functions->IterateOverObjectsReachableFromObject(
      arg_0, arg_1, arg_2_host_fn_ptr, arg_3);
}

void DoTrampoline_jvmtiEnv_IterateOverReachableObjects(HostCode /* callee */, ProcessState* state) {
  LOG_JNI("DoTrampoline_jvmtiEnv_IterateOverReachableObjects called");
  using PFN_callee = decltype(std::declval<jvmtiEnv>().functions->IterateOverReachableObjects);
  auto [arg_env, arg_1, arg_2, arg_3, arg_4] = GuestParamsValues<PFN_callee>(state);
  jvmtiEnv* arg_0 = ToHostJvmtiEnv(arg_env);
  auto&& [ret] = GuestReturnReference<PFN_callee>(state);

  auto arg_1_host_fn_ptr =
      WrapGuestFunction(arg_1, "IterateOverReachableObjects-jvmtiHeapRootCallback");
  auto arg_2_host_fn_ptr =
      WrapGuestFunction(arg_2, "IterateOverReachableObjects-jvmtiStackReferenceCallback");
  auto arg_3_host_fn_ptr =
      WrapGuestFunction(arg_3, "IterateOverReachableObjects-jvmtiObjectReferenceCallback");
  ret = arg_0->functions->IterateOverReachableObjects(
      arg_0, arg_1_host_fn_ptr, arg_2_host_fn_ptr, arg_3_host_fn_ptr, arg_4);
}

void DoTrampoline_jvmtiEnv_IterateOverHeap(HostCode /* callee */, ProcessState* state) {
  LOG_JNI("DoTrampoline_jvmtiEnv_IterateOverHeap called");
  using PFN_callee = decltype(std::declval<jvmtiEnv>().functions->IterateOverHeap);
  auto [arg_env, arg_1, arg_2, arg_3] = GuestParamsValues<PFN_callee>(state);
  jvmtiEnv* arg_0 = ToHostJvmtiEnv(arg_env);
  auto&& [ret] = GuestReturnReference<PFN_callee>(state);

  auto arg_2_host_fn_ptr = WrapGuestFunction(arg_2, "IterateOverHeap-jvmtiHeapObjectCallback");
  ret = arg_0->functions->IterateOverHeap(arg_0, arg_1, arg_2_host_fn_ptr, arg_3);
}

void DoTrampoline_jvmtiEnv_IterateOverInstancesOfClass(HostCode /* callee */, ProcessState* state) {
  LOG_JNI("DoTrampoline_jvmtiEnv_IterateOverInstancesOfClass called");
  using PFN_callee = decltype(std::declval<jvmtiEnv>().functions->IterateOverInstancesOfClass);
  auto [arg_env, arg_1, arg_2, arg_3, arg_4] = GuestParamsValues<PFN_callee>(state);
  jvmtiEnv* arg_0 = ToHostJvmtiEnv(arg_env);
  auto&& [ret] = GuestReturnReference<PFN_callee>(state);

  auto arg_3_host_fn_ptr =
      WrapGuestFunction(arg_3, "IterateOverInstancesOfClass-jvmtiHeapObjectCallback");
  ret =
      arg_0->functions->IterateOverInstancesOfClass(arg_0, arg_1, arg_2, arg_3_host_fn_ptr, arg_4);
}

void DoTrampoline_jvmtiEnv_FollowReferences(HostCode /* callee */, ProcessState* state) {
  LOG_JNI("DoTrampoline_jvmtiEnv_FollowReferences called");
  using PFN_callee = decltype(std::declval<jvmtiEnv>().functions->FollowReferences);
  auto [arg_env, arg_1, arg_2, arg_3, callbacks, arg_5] = GuestParamsValues<PFN_callee>(state);
  jvmtiEnv* arg_0 = ToHostJvmtiEnv(arg_env);
  auto&& [ret] = GuestReturnReference<PFN_callee>(state);

  auto host_callbacks = WrapGuestJvmtiHeapCallbacks(callbacks);
  ret = arg_0->functions->FollowReferences(arg_0, arg_1, arg_2, arg_3, &host_callbacks, arg_5);
}

void DoTrampoline_jvmtiEnv_IterateThroughHeap(HostCode /* callee */, ProcessState* state) {
  LOG_JNI("DoTrampoline_jvmtiEnv_IterateThroughHeap called");
  using PFN_callee = decltype(std::declval<jvmtiEnv>().functions->IterateThroughHeap);
  auto [arg_env, arg_1, arg_2, callbacks, arg_4] = GuestParamsValues<PFN_callee>(state);
  jvmtiEnv* arg_0 = ToHostJvmtiEnv(arg_env);
  auto&& [ret] = GuestReturnReference<PFN_callee>(state);

  auto host_callbacks = WrapGuestJvmtiHeapCallbacks(callbacks);
  ret = arg_0->functions->IterateThroughHeap(arg_0, arg_1, arg_2, &host_callbacks, arg_4);
}

void DoTrampoline_jvmtiEnv_SetEventCallbacks(HostCode /* callee */, ProcessState* state) {
  LOG_JNI("DoTrampoline_jvmtiEnv_SetEventCallbacks called");
  using PFN_callee = decltype(std::declval<jvmtiEnv>().functions->SetEventCallbacks);
  auto [arg_env, guest_callbacks, arg_2] = GuestParamsValues<PFN_callee>(state);
  jvmtiEnv* arg_0 = ToHostJvmtiEnv(arg_env);
  auto&& [ret] = GuestReturnReference<PFN_callee>(state);

  auto host_callbacks = WrapGuestJvmtiEventCallbacks(guest_callbacks);
  ret = arg_0->functions->SetEventCallbacks(arg_0, &host_callbacks, arg_2);
}

void DoTrampoline_jvmtiEnv_GetExtensionFunctions(HostCode /* callee */, ProcessState* state) {
  LOG_JNI("DoTrampoline_jvmtiEnv_GetExtensionFunctions called");
  using PFN_callee = decltype(std::declval<jvmtiEnv>().functions->GetExtensionFunctions);
  auto [arg_env, arg_count_ptr, arg_extensions_ptr] = GuestParamsValues<PFN_callee>(state);
  jvmtiEnv* host_jvmti_env = ToHostJvmtiEnv(arg_env);

  auto&& [ret] = GuestReturnReference<PFN_callee>(state);

  ret = host_jvmti_env->functions->GetExtensionFunctions(
      host_jvmti_env, arg_count_ptr, arg_extensions_ptr);

  if (ret == JVMTI_ERROR_NONE) {
    WrapJvmtiExtensionFunctionInfosIfNeeded(*arg_count_ptr, *arg_extensions_ptr);
  }
}
