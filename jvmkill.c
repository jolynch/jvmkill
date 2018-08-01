/*
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <sys/types.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include <jvmti.h>

#define NANOS 1000000000L

static struct timespec last_start, last_end;
static unsigned long val;

static void JNICALL
resourceExhausted(
      jvmtiEnv *jvmti_env,
      JNIEnv *jni_env,
      jint flags,
      const void *reserved,
      const char *description)
{
   fprintf(stderr,
      "ResourceExhausted: %s: killing current process!\n", description);
   kill(getpid(), SIGKILL);
}

static void JNICALL
gcStarted(jvmtiEnv *jvmti_env)
{
    clock_gettime(CLOCK_MONOTONIC, &last_start);
    long running_nanos = NANOS * (last_start.tv_sec - last_end.tv_sec) + (last_start.tv_nsec - last_end.tv_nsec);
    if (running_nanos > val) { val = 0; }
    else { val -= running_nanos; }

    fprintf(stderr, "Got a GC Start!\n");
    fprintf(stderr, "val %ld\n", val);
}

static void JNICALL
gcFinished(jvmtiEnv *jvmti_env)
{
    clock_gettime(CLOCK_MONOTONIC, &last_end);

    long gc_nanos = NANOS * (last_end.tv_sec - last_start.tv_sec) + (last_end.tv_nsec - last_start.tv_nsec);
    fprintf(stderr, "wat %ld\n", gc_nanos);
    val += gc_nanos;

    if (val > (NANOS * 30)) {
        // Trigger kill due to excessive GC
       fprintf(stderr,
          "Excessive GC: killing current process!\n");
        kill(getpid(), SIGKILL);
    }


    fprintf(stderr, "Got a GC End!\n");
    fprintf(stderr, "val %ld\n", val);
}

JNIEXPORT jint JNICALL
Agent_OnLoad(JavaVM *vm, char *options, void *reserved)
{
   jvmtiEnv *jvmti;
   jvmtiError err;

   clock_gettime(CLOCK_MONOTONIC, &last_start);
   clock_gettime(CLOCK_MONOTONIC, &last_end);

   jint rc = (*vm)->GetEnv(vm, (void **) &jvmti, JVMTI_VERSION);
   if (rc != JNI_OK) {
      fprintf(stderr, "ERROR: GetEnv failed: %d\n", rc);
      return JNI_ERR;
   }

   jvmtiEventCallbacks callbacks;
   memset(&callbacks, 0, sizeof(callbacks));

   callbacks.ResourceExhausted = &resourceExhausted;
   callbacks.GarbageCollectionStart = &gcStarted;
   callbacks.GarbageCollectionFinish = &gcFinished;

   err = (*jvmti)->SetEventCallbacks(jvmti, &callbacks, sizeof(callbacks));
   if (err != JVMTI_ERROR_NONE) {
      fprintf(stderr, "ERROR: SetEventCallbacks failed: %d\n", err);
      return JNI_ERR;
   }

   err = (*jvmti)->SetEventNotificationMode(
         jvmti, JVMTI_ENABLE, JVMTI_EVENT_RESOURCE_EXHAUSTED, NULL);
   if (err != JVMTI_ERROR_NONE) {
      fprintf(stderr, "ERROR: SetEventNotificationMode failed: %d\n", err);
      return JNI_ERR;
   }

   // Ask for ability to get GC events
   jvmtiCapabilities   capabilities;
   (void)memset(&capabilities, 0, sizeof(capabilities));
   capabilities.can_generate_garbage_collection_events = 1;
   err = (*jvmti)->AddCapabilities(jvmti, &capabilities);

   err = (*jvmti)->SetEventNotificationMode(
         jvmti, JVMTI_ENABLE, JVMTI_EVENT_GARBAGE_COLLECTION_START, NULL);
   if (err != JVMTI_ERROR_NONE) {
      fprintf(stderr, "ERROR: SetEventNotificationMode GC Start failed: %d\n", err);
      return JNI_ERR;
   }

   err = (*jvmti)->SetEventNotificationMode(
         jvmti, JVMTI_ENABLE, JVMTI_EVENT_GARBAGE_COLLECTION_FINISH, NULL);
   if (err != JVMTI_ERROR_NONE) {
      fprintf(stderr, "ERROR: SetEventNotificationMode GC End failed: %d\n", err);
      return JNI_ERR;
   }



   return JNI_OK;
}
