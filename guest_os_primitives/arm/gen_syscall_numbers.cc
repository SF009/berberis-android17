/*
 * Copyright (C) 2024 The Android Open Source Project
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

#include "berberis/guest_os_primitives/gen_syscall_numbers.h"

namespace berberis {

int ToHostSyscallNumber(int nr) {
  switch (nr) {
    case 140:  // __NR__llseek
      return 140;
    case 142:  // __NR__newselect
      return 142;
    case 149:  // __NR__sysctl
      return 149;
    case 285:  // __NR_accept - missing on x86
      return -1;
    case 366:  // __NR_accept4
      return 364;
    case 33:  // __NR_access
      return 33;
    case 51:  // __NR_acct
      return 51;
    case 309:  // __NR_add_key
      return 286;
    case 124:  // __NR_adjtimex
      return 124;
    case 270:  // __NR_arm_fadvise64_64 - missing on x86
      return -1;
    case 341:  // __NR_arm_sync_file_range - missing on x86
      return -1;
    case 134:  // __NR_bdflush
      return 134;
    case 282:  // __NR_bind
      return 361;
    case 386:  // __NR_bpf
      return 357;
    case 45:  // __NR_brk
      return 45;
    case 184:  // __NR_capget
      return 184;
    case 185:  // __NR_capset
      return 185;
    case 12:  // __NR_chdir
      return 12;
    case 15:  // __NR_chmod
      return 15;
    case 182:  // __NR_chown
      return 182;
    case 212:  // __NR_chown32
      return 212;
    case 61:  // __NR_chroot
      return 61;
    case 372:  // __NR_clock_adjtime
      return 343;
    case 405:  // __NR_clock_adjtime64
      return 405;
    case 264:  // __NR_clock_getres
      return 266;
    case 406:  // __NR_clock_getres_time64
      return 406;
    case 263:  // __NR_clock_gettime
      return 265;
    case 403:  // __NR_clock_gettime64
      return 403;
    case 265:  // __NR_clock_nanosleep
      return 267;
    case 407:  // __NR_clock_nanosleep_time64
      return 407;
    case 262:  // __NR_clock_settime
      return 264;
    case 404:  // __NR_clock_settime64
      return 404;
    case 120:  // __NR_clone
      return 120;
    case 435:  // __NR_clone3
      return 435;
    case 6:  // __NR_close
      return 6;
    case 436:  // __NR_close_range
      return 436;
    case 283:  // __NR_connect
      return 362;
    case 391:  // __NR_copy_file_range
      return 377;
    case 8:  // __NR_creat
      return 8;
    case 129:  // __NR_delete_module
      return 129;
    case 41:  // __NR_dup
      return 41;
    case 63:  // __NR_dup2
      return 63;
    case 358:  // __NR_dup3
      return 330;
    case 250:  // __NR_epoll_create
      return 254;
    case 357:  // __NR_epoll_create1
      return 329;
    case 251:  // __NR_epoll_ctl
      return 255;
    case 346:  // __NR_epoll_pwait
      return 319;
    case 441:  // __NR_epoll_pwait2
      return 441;
    case 252:  // __NR_epoll_wait
      return 256;
    case 351:  // __NR_eventfd
      return 323;
    case 356:  // __NR_eventfd2
      return 328;
    case 11:  // __NR_execve
      return 11;
    case 387:  // __NR_execveat
      return 358;
    case 1:  // __NR_exit
      return 1;
    case 248:  // __NR_exit_group
      return 252;
    case 334:  // __NR_faccessat
      return 307;
    case 439:  // __NR_faccessat2
      return 439;
    case 352:  // __NR_fallocate
      return 324;
    case 367:  // __NR_fanotify_init
      return 338;
    case 368:  // __NR_fanotify_mark
      return 339;
    case 133:  // __NR_fchdir
      return 133;
    case 94:  // __NR_fchmod
      return 94;
    case 333:  // __NR_fchmodat
      return 306;
    case 95:  // __NR_fchown
      return 95;
    case 207:  // __NR_fchown32
      return 207;
    case 325:  // __NR_fchownat
      return 298;
    case 55:  // __NR_fcntl
      return 55;
    case 221:  // __NR_fcntl64
      return 221;
    case 148:  // __NR_fdatasync
      return 148;
    case 231:  // __NR_fgetxattr
      return 231;
    case 379:  // __NR_finit_module
      return 350;
    case 234:  // __NR_flistxattr
      return 234;
    case 143:  // __NR_flock
      return 143;
    case 2:  // __NR_fork
      return 2;
    case 237:  // __NR_fremovexattr
      return 237;
    case 431:  // __NR_fsconfig
      return 431;
    case 228:  // __NR_fsetxattr
      return 228;
    case 432:  // __NR_fsmount
      return 432;
    case 430:  // __NR_fsopen
      return 430;
    case 433:  // __NR_fspick
      return 433;
    case 108:  // __NR_fstat
      return 108;
    case 197:  // __NR_fstat64
      return 197;
    case 327:  // __NR_fstatat64
      return 300;
    case 100:  // __NR_fstatfs
      return 100;
    case 267:  // __NR_fstatfs64
      return 269;
    case 118:  // __NR_fsync
      return 118;
    case 93:  // __NR_ftruncate
      return 93;
    case 194:  // __NR_ftruncate64
      return 194;
    case 240:  // __NR_futex
      return 240;
    case 422:  // __NR_futex_time64
      return 422;
    case 449:  // __NR_futex_waitv
      return 449;
    case 326:  // __NR_futimesat
      return 299;
    case 320:  // __NR_get_mempolicy
      return 275;
    case 339:  // __NR_get_robust_list
      return 312;
    case 345:  // __NR_getcpu
      return 318;
    case 183:  // __NR_getcwd
      return 183;
    case 141:  // __NR_getdents
      return 141;
    case 217:  // __NR_getdents64
      return 220;
    case 50:  // __NR_getegid
      return 50;
    case 202:  // __NR_getegid32
      return 202;
    case 49:  // __NR_geteuid
      return 49;
    case 201:  // __NR_geteuid32
      return 201;
    case 47:  // __NR_getgid
      return 47;
    case 200:  // __NR_getgid32
      return 200;
    case 80:  // __NR_getgroups
      return 80;
    case 205:  // __NR_getgroups32
      return 205;
    case 105:  // __NR_getitimer
      return 105;
    case 287:  // __NR_getpeername
      return 368;
    case 132:  // __NR_getpgid
      return 132;
    case 65:  // __NR_getpgrp
      return 65;
    case 20:  // __NR_getpid
      return 20;
    case 64:  // __NR_getppid
      return 64;
    case 96:  // __NR_getpriority
      return 96;
    case 384:  // __NR_getrandom
      return 355;
    case 171:  // __NR_getresgid
      return 171;
    case 211:  // __NR_getresgid32
      return 211;
    case 165:  // __NR_getresuid
      return 165;
    case 209:  // __NR_getresuid32
      return 209;
    case 77:  // __NR_getrusage
      return 77;
    case 147:  // __NR_getsid
      return 147;
    case 286:  // __NR_getsockname
      return 367;
    case 295:  // __NR_getsockopt
      return 365;
    case 224:  // __NR_gettid
      return 224;
    case 78:  // __NR_gettimeofday
      return 78;
    case 24:  // __NR_getuid
      return 24;
    case 199:  // __NR_getuid32
      return 199;
    case 229:  // __NR_getxattr
      return 229;
    case 128:  // __NR_init_module
      return 128;
    case 317:  // __NR_inotify_add_watch
      return 292;
    case 316:  // __NR_inotify_init
      return 291;
    case 360:  // __NR_inotify_init1
      return 332;
    case 318:  // __NR_inotify_rm_watch
      return 293;
    case 247:  // __NR_io_cancel
      return 249;
    case 244:  // __NR_io_destroy
      return 246;
    case 245:  // __NR_io_getevents
      return 247;
    case 399:  // __NR_io_pgetevents
      return 385;
    case 416:  // __NR_io_pgetevents_time64
      return 416;
    case 243:  // __NR_io_setup
      return 245;
    case 246:  // __NR_io_submit
      return 248;
    case 426:  // __NR_io_uring_enter
      return 426;
    case 427:  // __NR_io_uring_register
      return 427;
    case 425:  // __NR_io_uring_setup
      return 425;
    case 54:  // __NR_ioctl
      return 54;
    case 315:  // __NR_ioprio_get
      return 290;
    case 314:  // __NR_ioprio_set
      return 289;
    case 378:  // __NR_kcmp
      return 349;
    case 401:  // __NR_kexec_file_load - missing on x86
      return -1;
    case 347:  // __NR_kexec_load
      return 283;
    case 311:  // __NR_keyctl
      return 288;
    case 37:  // __NR_kill
      return 37;
    case 445:  // __NR_landlock_add_rule
      return 445;
    case 444:  // __NR_landlock_create_ruleset
      return 444;
    case 446:  // __NR_landlock_restrict_self
      return 446;
    case 16:  // __NR_lchown
      return 16;
    case 198:  // __NR_lchown32
      return 198;
    case 230:  // __NR_lgetxattr
      return 230;
    case 9:  // __NR_link
      return 9;
    case 330:  // __NR_linkat
      return 303;
    case 284:  // __NR_listen
      return 363;
    case 232:  // __NR_listxattr
      return 232;
    case 233:  // __NR_llistxattr
      return 233;
    case 249:  // __NR_lookup_dcookie
      return 253;
    case 236:  // __NR_lremovexattr
      return 236;
    case 19:  // __NR_lseek
      return 19;
    case 227:  // __NR_lsetxattr
      return 227;
    case 107:  // __NR_lstat
      return 107;
    case 196:  // __NR_lstat64
      return 196;
    case 220:  // __NR_madvise
      return 219;
    case 319:  // __NR_mbind
      return 274;
    case 389:  // __NR_membarrier
      return 375;
    case 385:  // __NR_memfd_create
      return 356;
    case 400:  // __NR_migrate_pages
      return 294;
    case 219:  // __NR_mincore
      return 218;
    case 39:  // __NR_mkdir
      return 39;
    case 323:  // __NR_mkdirat
      return 296;
    case 14:  // __NR_mknod
      return 14;
    case 324:  // __NR_mknodat
      return 297;
    case 150:  // __NR_mlock
      return 150;
    case 390:  // __NR_mlock2
      return 376;
    case 152:  // __NR_mlockall
      return 152;
    case 192:  // __NR_mmap2
      return 192;
    case 21:  // __NR_mount
      return 21;
    case 442:  // __NR_mount_setattr
      return 442;
    case 429:  // __NR_move_mount
      return 429;
    case 344:  // __NR_move_pages
      return 317;
    case 125:  // __NR_mprotect
      return 125;
    case 279:  // __NR_mq_getsetattr
      return 282;
    case 278:  // __NR_mq_notify
      return 281;
    case 274:  // __NR_mq_open
      return 277;
    case 277:  // __NR_mq_timedreceive
      return 280;
    case 419:  // __NR_mq_timedreceive_time64
      return 419;
    case 276:  // __NR_mq_timedsend
      return 279;
    case 418:  // __NR_mq_timedsend_time64
      return 418;
    case 275:  // __NR_mq_unlink
      return 278;
    case 163:  // __NR_mremap
      return 163;
    case 304:  // __NR_msgctl
      return 402;
    case 303:  // __NR_msgget
      return 399;
    case 302:  // __NR_msgrcv
      return 401;
    case 301:  // __NR_msgsnd
      return 400;
    case 144:  // __NR_msync
      return 144;
    case 151:  // __NR_munlock
      return 151;
    case 153:  // __NR_munlockall
      return 153;
    case 91:  // __NR_munmap
      return 91;
    case 370:  // __NR_name_to_handle_at
      return 341;
    case 162:  // __NR_nanosleep
      return 162;
    case 169:  // __NR_nfsservctl
      return 169;
    case 34:  // __NR_nice
      return 34;
    case 5:  // __NR_open
      return 5;
    case 371:  // __NR_open_by_handle_at
      return 342;
    case 428:  // __NR_open_tree
      return 428;
    case 322:  // __NR_openat
      return 295;
    case 437:  // __NR_openat2
      return 437;
    case 29:  // __NR_pause
      return 29;
    case 271:  // __NR_pciconfig_iobase - missing on x86
      return -1;
    case 272:  // __NR_pciconfig_read - missing on x86
      return -1;
    case 273:  // __NR_pciconfig_write - missing on x86
      return -1;
    case 364:  // __NR_perf_event_open
      return 336;
    case 136:  // __NR_personality
      return 136;
    case 438:  // __NR_pidfd_getfd
      return 438;
    case 434:  // __NR_pidfd_open
      return 434;
    case 424:  // __NR_pidfd_send_signal
      return 424;
    case 42:  // __NR_pipe
      return 42;
    case 359:  // __NR_pipe2
      return 331;
    case 218:  // __NR_pivot_root
      return 217;
    case 395:  // __NR_pkey_alloc
      return 381;
    case 396:  // __NR_pkey_free
      return 382;
    case 394:  // __NR_pkey_mprotect
      return 380;
    case 168:  // __NR_poll
      return 168;
    case 336:  // __NR_ppoll
      return 309;
    case 414:  // __NR_ppoll_time64
      return 414;
    case 172:  // __NR_prctl
      return 172;
    case 180:  // __NR_pread64
      return 180;
    case 361:  // __NR_preadv
      return 333;
    case 392:  // __NR_preadv2
      return 378;
    case 369:  // __NR_prlimit64
      return 340;
    case 440:  // __NR_process_madvise
      return 440;
    case 448:  // __NR_process_mrelease
      return 448;
    case 376:  // __NR_process_vm_readv
      return 347;
    case 377:  // __NR_process_vm_writev
      return 348;
    case 335:  // __NR_pselect6
      return 308;
    case 413:  // __NR_pselect6_time64
      return 413;
    case 26:  // __NR_ptrace
      return 26;
    case 181:  // __NR_pwrite64
      return 181;
    case 362:  // __NR_pwritev
      return 334;
    case 393:  // __NR_pwritev2
      return 379;
    case 131:  // __NR_quotactl
      return 131;
    case 443:  // __NR_quotactl_fd
      return 443;
    case 3:  // __NR_read
      return 3;
    case 225:  // __NR_readahead
      return 225;
    case 85:  // __NR_readlink
      return 85;
    case 332:  // __NR_readlinkat
      return 305;
    case 145:  // __NR_readv
      return 145;
    case 88:  // __NR_reboot
      return 88;
    case 291:  // __NR_recv - missing on x86
      return -1;
    case 292:  // __NR_recvfrom
      return 371;
    case 365:  // __NR_recvmmsg
      return 337;
    case 417:  // __NR_recvmmsg_time64
      return 417;
    case 297:  // __NR_recvmsg
      return 372;
    case 253:  // __NR_remap_file_pages
      return 257;
    case 235:  // __NR_removexattr
      return 235;
    case 38:  // __NR_rename
      return 38;
    case 329:  // __NR_renameat
      return 302;
    case 382:  // __NR_renameat2
      return 353;
    case 310:  // __NR_request_key
      return 287;
    case 0:  // __NR_restart_syscall
      return 0;
    case 40:  // __NR_rmdir
      return 40;
    case 398:  // __NR_rseq
      return 386;
    case 174:  // __NR_rt_sigaction
      return 174;
    case 176:  // __NR_rt_sigpending
      return 176;
    case 175:  // __NR_rt_sigprocmask
      return 175;
    case 178:  // __NR_rt_sigqueueinfo
      return 178;
    case 173:  // __NR_rt_sigreturn
      return 173;
    case 179:  // __NR_rt_sigsuspend
      return 179;
    case 177:  // __NR_rt_sigtimedwait
      return 177;
    case 421:  // __NR_rt_sigtimedwait_time64
      return 421;
    case 363:  // __NR_rt_tgsigqueueinfo
      return 335;
    case 159:  // __NR_sched_get_priority_max
      return 159;
    case 160:  // __NR_sched_get_priority_min
      return 160;
    case 242:  // __NR_sched_getaffinity
      return 242;
    case 381:  // __NR_sched_getattr
      return 352;
    case 155:  // __NR_sched_getparam
      return 155;
    case 157:  // __NR_sched_getscheduler
      return 157;
    case 161:  // __NR_sched_rr_get_interval
      return 161;
    case 423:  // __NR_sched_rr_get_interval_time64
      return 423;
    case 241:  // __NR_sched_setaffinity
      return 241;
    case 380:  // __NR_sched_setattr
      return 351;
    case 154:  // __NR_sched_setparam
      return 154;
    case 156:  // __NR_sched_setscheduler
      return 156;
    case 158:  // __NR_sched_yield
      return 158;
    case 383:  // __NR_seccomp
      return 354;
    case 300:  // __NR_semctl
      return 394;
    case 299:  // __NR_semget
      return 393;
    case 298:  // __NR_semop - missing on x86
      return -1;
    case 312:  // __NR_semtimedop - missing on x86
      return -1;
    case 420:  // __NR_semtimedop_time64
      return 420;
    case 289:  // __NR_send - missing on x86
      return -1;
    case 187:  // __NR_sendfile
      return 187;
    case 239:  // __NR_sendfile64
      return 239;
    case 374:  // __NR_sendmmsg
      return 345;
    case 296:  // __NR_sendmsg
      return 370;
    case 290:  // __NR_sendto
      return 369;
    case 321:  // __NR_set_mempolicy
      return 276;
    case 450:  // __NR_set_mempolicy_home_node
      return 450;
    case 338:  // __NR_set_robust_list
      return 311;
    case 256:  // __NR_set_tid_address
      return 258;
    case 121:  // __NR_setdomainname
      return 121;
    case 139:  // __NR_setfsgid
      return 139;
    case 216:  // __NR_setfsgid32
      return 216;
    case 138:  // __NR_setfsuid
      return 138;
    case 215:  // __NR_setfsuid32
      return 215;
    case 46:  // __NR_setgid
      return 46;
    case 214:  // __NR_setgid32
      return 214;
    case 81:  // __NR_setgroups
      return 81;
    case 206:  // __NR_setgroups32
      return 206;
    case 74:  // __NR_sethostname
      return 74;
    case 104:  // __NR_setitimer
      return 104;
    case 375:  // __NR_setns
      return 346;
    case 57:  // __NR_setpgid
      return 57;
    case 97:  // __NR_setpriority
      return 97;
    case 71:  // __NR_setregid
      return 71;
    case 204:  // __NR_setregid32
      return 204;
    case 170:  // __NR_setresgid
      return 170;
    case 210:  // __NR_setresgid32
      return 210;
    case 164:  // __NR_setresuid
      return 164;
    case 208:  // __NR_setresuid32
      return 208;
    case 70:  // __NR_setreuid
      return 70;
    case 203:  // __NR_setreuid32
      return 203;
    case 75:  // __NR_setrlimit
      return 75;
    case 66:  // __NR_setsid
      return 66;
    case 294:  // __NR_setsockopt
      return 366;
    case 79:  // __NR_settimeofday
      return 79;
    case 23:  // __NR_setuid
      return 23;
    case 213:  // __NR_setuid32
      return 213;
    case 226:  // __NR_setxattr
      return 226;
    case 305:  // __NR_shmat
      return 397;
    case 308:  // __NR_shmctl
      return 396;
    case 306:  // __NR_shmdt
      return 398;
    case 307:  // __NR_shmget
      return 395;
    case 293:  // __NR_shutdown
      return 373;
    case 67:  // __NR_sigaction
      return 67;
    case 186:  // __NR_sigaltstack
      return 186;
    case 349:  // __NR_signalfd
      return 321;
    case 355:  // __NR_signalfd4
      return 327;
    case 73:  // __NR_sigpending
      return 73;
    case 126:  // __NR_sigprocmask
      return 126;
    case 119:  // __NR_sigreturn
      return 119;
    case 72:  // __NR_sigsuspend
      return 72;
    case 281:  // __NR_socket
      return 359;
    case 288:  // __NR_socketpair
      return 360;
    case 340:  // __NR_splice
      return 313;
    case 106:  // __NR_stat
      return 106;
    case 195:  // __NR_stat64
      return 195;
    case 99:  // __NR_statfs
      return 99;
    case 266:  // __NR_statfs64
      return 268;
    case 397:  // __NR_statx
      return 383;
    case 115:  // __NR_swapoff
      return 115;
    case 87:  // __NR_swapon
      return 87;
    case 83:  // __NR_symlink
      return 83;
    case 331:  // __NR_symlinkat
      return 304;
    case 36:  // __NR_sync
      return 36;
    case 373:  // __NR_syncfs
      return 344;
    case 135:  // __NR_sysfs
      return 135;
    case 116:  // __NR_sysinfo
      return 116;
    case 103:  // __NR_syslog
      return 103;
    case 342:  // __NR_tee
      return 315;
    case 268:  // __NR_tgkill
      return 270;
    case 257:  // __NR_timer_create
      return 259;
    case 261:  // __NR_timer_delete
      return 263;
    case 260:  // __NR_timer_getoverrun
      return 262;
    case 259:  // __NR_timer_gettime
      return 261;
    case 408:  // __NR_timer_gettime64
      return 408;
    case 258:  // __NR_timer_settime
      return 260;
    case 409:  // __NR_timer_settime64
      return 409;
    case 350:  // __NR_timerfd_create
      return 322;
    case 354:  // __NR_timerfd_gettime
      return 326;
    case 410:  // __NR_timerfd_gettime64
      return 410;
    case 353:  // __NR_timerfd_settime
      return 325;
    case 411:  // __NR_timerfd_settime64
      return 411;
    case 43:  // __NR_times
      return 43;
    case 238:  // __NR_tkill
      return 238;
    case 92:  // __NR_truncate
      return 92;
    case 193:  // __NR_truncate64
      return 193;
    case 191:  // __NR_ugetrlimit
      return 191;
    case 60:  // __NR_umask
      return 60;
    case 52:  // __NR_umount2
      return 52;
    case 122:  // __NR_uname
      return 122;
    case 10:  // __NR_unlink
      return 10;
    case 328:  // __NR_unlinkat
      return 301;
    case 337:  // __NR_unshare
      return 310;
    case 86:  // __NR_uselib
      return 86;
    case 388:  // __NR_userfaultfd
      return 374;
    case 62:  // __NR_ustat
      return 62;
    case 348:  // __NR_utimensat
      return 320;
    case 412:  // __NR_utimensat_time64
      return 412;
    case 269:  // __NR_utimes
      return 271;
    case 190:  // __NR_vfork
      return 190;
    case 111:  // __NR_vhangup
      return 111;
    case 343:  // __NR_vmsplice
      return 316;
    case 313:  // __NR_vserver
      return 273;
    case 114:  // __NR_wait4
      return 114;
    case 280:  // __NR_waitid
      return 284;
    case 4:  // __NR_write
      return 4;
    case 146:  // __NR_writev
      return 146;
    default:
      return -1;
  }
}

int ToGuestSyscallNumber(int nr) {
  switch (nr) {
    case 140:  // __NR__llseek
      return 140;
    case 142:  // __NR__newselect
      return 142;
    case 149:  // __NR__sysctl
      return 149;
    case 364:  // __NR_accept4
      return 366;
    case 33:  // __NR_access
      return 33;
    case 51:  // __NR_acct
      return 51;
    case 286:  // __NR_add_key
      return 309;
    case 124:  // __NR_adjtimex
      return 124;
    case 137:  // __NR_afs_syscall - missing on arm
      return -1;
    case 27:  // __NR_alarm - missing on arm
      return -1;
    case 384:  // __NR_arch_prctl - missing on arm
      return -1;
    case 134:  // __NR_bdflush
      return 134;
    case 361:  // __NR_bind
      return 282;
    case 357:  // __NR_bpf
      return 386;
    case 17:  // __NR_break - missing on arm
      return -1;
    case 45:  // __NR_brk
      return 45;
    case 184:  // __NR_capget
      return 184;
    case 185:  // __NR_capset
      return 185;
    case 12:  // __NR_chdir
      return 12;
    case 15:  // __NR_chmod
      return 15;
    case 182:  // __NR_chown
      return 182;
    case 212:  // __NR_chown32
      return 212;
    case 61:  // __NR_chroot
      return 61;
    case 343:  // __NR_clock_adjtime
      return 372;
    case 405:  // __NR_clock_adjtime64
      return 405;
    case 266:  // __NR_clock_getres
      return 264;
    case 406:  // __NR_clock_getres_time64
      return 406;
    case 265:  // __NR_clock_gettime
      return 263;
    case 403:  // __NR_clock_gettime64
      return 403;
    case 267:  // __NR_clock_nanosleep
      return 265;
    case 407:  // __NR_clock_nanosleep_time64
      return 407;
    case 264:  // __NR_clock_settime
      return 262;
    case 404:  // __NR_clock_settime64
      return 404;
    case 120:  // __NR_clone
      return 120;
    case 435:  // __NR_clone3
      return 435;
    case 6:  // __NR_close
      return 6;
    case 436:  // __NR_close_range
      return 436;
    case 362:  // __NR_connect
      return 283;
    case 377:  // __NR_copy_file_range
      return 391;
    case 8:  // __NR_creat
      return 8;
    case 127:  // __NR_create_module - missing on arm
      return -1;
    case 129:  // __NR_delete_module
      return 129;
    case 41:  // __NR_dup
      return 41;
    case 63:  // __NR_dup2
      return 63;
    case 330:  // __NR_dup3
      return 358;
    case 254:  // __NR_epoll_create
      return 250;
    case 329:  // __NR_epoll_create1
      return 357;
    case 255:  // __NR_epoll_ctl
      return 251;
    case 319:  // __NR_epoll_pwait
      return 346;
    case 441:  // __NR_epoll_pwait2
      return 441;
    case 256:  // __NR_epoll_wait
      return 252;
    case 323:  // __NR_eventfd
      return 351;
    case 328:  // __NR_eventfd2
      return 356;
    case 11:  // __NR_execve
      return 11;
    case 358:  // __NR_execveat
      return 387;
    case 1:  // __NR_exit
      return 1;
    case 252:  // __NR_exit_group
      return 248;
    case 307:  // __NR_faccessat
      return 334;
    case 439:  // __NR_faccessat2
      return 439;
    case 250:  // __NR_fadvise64 - missing on arm
      return -1;
    case 272:  // __NR_fadvise64_64 - missing on arm
      return -1;
    case 324:  // __NR_fallocate
      return 352;
    case 338:  // __NR_fanotify_init
      return 367;
    case 339:  // __NR_fanotify_mark
      return 368;
    case 133:  // __NR_fchdir
      return 133;
    case 94:  // __NR_fchmod
      return 94;
    case 306:  // __NR_fchmodat
      return 333;
    case 95:  // __NR_fchown
      return 95;
    case 207:  // __NR_fchown32
      return 207;
    case 298:  // __NR_fchownat
      return 325;
    case 55:  // __NR_fcntl
      return 55;
    case 221:  // __NR_fcntl64
      return 221;
    case 148:  // __NR_fdatasync
      return 148;
    case 231:  // __NR_fgetxattr
      return 231;
    case 350:  // __NR_finit_module
      return 379;
    case 234:  // __NR_flistxattr
      return 234;
    case 143:  // __NR_flock
      return 143;
    case 2:  // __NR_fork
      return 2;
    case 237:  // __NR_fremovexattr
      return 237;
    case 431:  // __NR_fsconfig
      return 431;
    case 228:  // __NR_fsetxattr
      return 228;
    case 432:  // __NR_fsmount
      return 432;
    case 430:  // __NR_fsopen
      return 430;
    case 433:  // __NR_fspick
      return 433;
    case 108:  // __NR_fstat
      return 108;
    case 197:  // __NR_fstat64
      return 197;
    case 300:  // __NR_fstatat64
      return 327;
    case 100:  // __NR_fstatfs
      return 100;
    case 269:  // __NR_fstatfs64
      return 267;
    case 118:  // __NR_fsync
      return 118;
    case 35:  // __NR_ftime - missing on arm
      return -1;
    case 93:  // __NR_ftruncate
      return 93;
    case 194:  // __NR_ftruncate64
      return 194;
    case 240:  // __NR_futex
      return 240;
    case 422:  // __NR_futex_time64
      return 422;
    case 449:  // __NR_futex_waitv
      return 449;
    case 299:  // __NR_futimesat
      return 326;
    case 130:  // __NR_get_kernel_syms - missing on arm
      return -1;
    case 275:  // __NR_get_mempolicy
      return 320;
    case 312:  // __NR_get_robust_list
      return 339;
    case 244:  // __NR_get_thread_area - missing on arm
      return -1;
    case 318:  // __NR_getcpu
      return 345;
    case 183:  // __NR_getcwd
      return 183;
    case 141:  // __NR_getdents
      return 141;
    case 220:  // __NR_getdents64
      return 217;
    case 50:  // __NR_getegid
      return 50;
    case 202:  // __NR_getegid32
      return 202;
    case 49:  // __NR_geteuid
      return 49;
    case 201:  // __NR_geteuid32
      return 201;
    case 47:  // __NR_getgid
      return 47;
    case 200:  // __NR_getgid32
      return 200;
    case 80:  // __NR_getgroups
      return 80;
    case 205:  // __NR_getgroups32
      return 205;
    case 105:  // __NR_getitimer
      return 105;
    case 368:  // __NR_getpeername
      return 287;
    case 132:  // __NR_getpgid
      return 132;
    case 65:  // __NR_getpgrp
      return 65;
    case 20:  // __NR_getpid
      return 20;
    case 188:  // __NR_getpmsg - missing on arm
      return -1;
    case 64:  // __NR_getppid
      return 64;
    case 96:  // __NR_getpriority
      return 96;
    case 355:  // __NR_getrandom
      return 384;
    case 171:  // __NR_getresgid
      return 171;
    case 211:  // __NR_getresgid32
      return 211;
    case 165:  // __NR_getresuid
      return 165;
    case 209:  // __NR_getresuid32
      return 209;
    case 76:  // __NR_getrlimit - missing on arm
      return -1;
    case 77:  // __NR_getrusage
      return 77;
    case 147:  // __NR_getsid
      return 147;
    case 367:  // __NR_getsockname
      return 286;
    case 365:  // __NR_getsockopt
      return 295;
    case 224:  // __NR_gettid
      return 224;
    case 78:  // __NR_gettimeofday
      return 78;
    case 24:  // __NR_getuid
      return 24;
    case 199:  // __NR_getuid32
      return 199;
    case 229:  // __NR_getxattr
      return 229;
    case 32:  // __NR_gtty - missing on arm
      return -1;
    case 112:  // __NR_idle - missing on arm
      return -1;
    case 128:  // __NR_init_module
      return 128;
    case 292:  // __NR_inotify_add_watch
      return 317;
    case 291:  // __NR_inotify_init
      return 316;
    case 332:  // __NR_inotify_init1
      return 360;
    case 293:  // __NR_inotify_rm_watch
      return 318;
    case 249:  // __NR_io_cancel
      return 247;
    case 246:  // __NR_io_destroy
      return 244;
    case 247:  // __NR_io_getevents
      return 245;
    case 385:  // __NR_io_pgetevents
      return 399;
    case 416:  // __NR_io_pgetevents_time64
      return 416;
    case 245:  // __NR_io_setup
      return 243;
    case 248:  // __NR_io_submit
      return 246;
    case 426:  // __NR_io_uring_enter
      return 426;
    case 427:  // __NR_io_uring_register
      return 427;
    case 425:  // __NR_io_uring_setup
      return 425;
    case 54:  // __NR_ioctl
      return 54;
    case 101:  // __NR_ioperm - missing on arm
      return -1;
    case 110:  // __NR_iopl - missing on arm
      return -1;
    case 290:  // __NR_ioprio_get
      return 315;
    case 289:  // __NR_ioprio_set
      return 314;
    case 117:  // __NR_ipc - missing on arm
      return -1;
    case 349:  // __NR_kcmp
      return 378;
    case 283:  // __NR_kexec_load
      return 347;
    case 288:  // __NR_keyctl
      return 311;
    case 37:  // __NR_kill
      return 37;
    case 445:  // __NR_landlock_add_rule
      return 445;
    case 444:  // __NR_landlock_create_ruleset
      return 444;
    case 446:  // __NR_landlock_restrict_self
      return 446;
    case 16:  // __NR_lchown
      return 16;
    case 198:  // __NR_lchown32
      return 198;
    case 230:  // __NR_lgetxattr
      return 230;
    case 9:  // __NR_link
      return 9;
    case 303:  // __NR_linkat
      return 330;
    case 363:  // __NR_listen
      return 284;
    case 232:  // __NR_listxattr
      return 232;
    case 233:  // __NR_llistxattr
      return 233;
    case 53:  // __NR_lock - missing on arm
      return -1;
    case 253:  // __NR_lookup_dcookie
      return 249;
    case 236:  // __NR_lremovexattr
      return 236;
    case 19:  // __NR_lseek
      return 19;
    case 227:  // __NR_lsetxattr
      return 227;
    case 107:  // __NR_lstat
      return 107;
    case 196:  // __NR_lstat64
      return 196;
    case 219:  // __NR_madvise
      return 220;
    case 274:  // __NR_mbind
      return 319;
    case 375:  // __NR_membarrier
      return 389;
    case 356:  // __NR_memfd_create
      return 385;
    case 447:  // __NR_memfd_secret - missing on arm
      return -1;
    case 294:  // __NR_migrate_pages
      return 400;
    case 218:  // __NR_mincore
      return 219;
    case 39:  // __NR_mkdir
      return 39;
    case 296:  // __NR_mkdirat
      return 323;
    case 14:  // __NR_mknod
      return 14;
    case 297:  // __NR_mknodat
      return 324;
    case 150:  // __NR_mlock
      return 150;
    case 376:  // __NR_mlock2
      return 390;
    case 152:  // __NR_mlockall
      return 152;
    case 90:  // __NR_mmap - missing on arm
      return -1;
    case 192:  // __NR_mmap2
      return 192;
    case 123:  // __NR_modify_ldt - missing on arm
      return -1;
    case 21:  // __NR_mount
      return 21;
    case 442:  // __NR_mount_setattr
      return 442;
    case 429:  // __NR_move_mount
      return 429;
    case 317:  // __NR_move_pages
      return 344;
    case 125:  // __NR_mprotect
      return 125;
    case 56:  // __NR_mpx - missing on arm
      return -1;
    case 282:  // __NR_mq_getsetattr
      return 279;
    case 281:  // __NR_mq_notify
      return 278;
    case 277:  // __NR_mq_open
      return 274;
    case 280:  // __NR_mq_timedreceive
      return 277;
    case 419:  // __NR_mq_timedreceive_time64
      return 419;
    case 279:  // __NR_mq_timedsend
      return 276;
    case 418:  // __NR_mq_timedsend_time64
      return 418;
    case 278:  // __NR_mq_unlink
      return 275;
    case 163:  // __NR_mremap
      return 163;
    case 402:  // __NR_msgctl
      return 304;
    case 399:  // __NR_msgget
      return 303;
    case 401:  // __NR_msgrcv
      return 302;
    case 400:  // __NR_msgsnd
      return 301;
    case 144:  // __NR_msync
      return 144;
    case 151:  // __NR_munlock
      return 151;
    case 153:  // __NR_munlockall
      return 153;
    case 91:  // __NR_munmap
      return 91;
    case 341:  // __NR_name_to_handle_at
      return 370;
    case 162:  // __NR_nanosleep
      return 162;
    case 169:  // __NR_nfsservctl
      return 169;
    case 34:  // __NR_nice
      return 34;
    case 28:  // __NR_oldfstat - missing on arm
      return -1;
    case 84:  // __NR_oldlstat - missing on arm
      return -1;
    case 59:  // __NR_oldolduname - missing on arm
      return -1;
    case 18:  // __NR_oldstat - missing on arm
      return -1;
    case 109:  // __NR_olduname - missing on arm
      return -1;
    case 5:  // __NR_open
      return 5;
    case 342:  // __NR_open_by_handle_at
      return 371;
    case 428:  // __NR_open_tree
      return 428;
    case 295:  // __NR_openat
      return 322;
    case 437:  // __NR_openat2
      return 437;
    case 29:  // __NR_pause
      return 29;
    case 336:  // __NR_perf_event_open
      return 364;
    case 136:  // __NR_personality
      return 136;
    case 438:  // __NR_pidfd_getfd
      return 438;
    case 434:  // __NR_pidfd_open
      return 434;
    case 424:  // __NR_pidfd_send_signal
      return 424;
    case 42:  // __NR_pipe
      return 42;
    case 331:  // __NR_pipe2
      return 359;
    case 217:  // __NR_pivot_root
      return 218;
    case 381:  // __NR_pkey_alloc
      return 395;
    case 382:  // __NR_pkey_free
      return 396;
    case 380:  // __NR_pkey_mprotect
      return 394;
    case 168:  // __NR_poll
      return 168;
    case 309:  // __NR_ppoll
      return 336;
    case 414:  // __NR_ppoll_time64
      return 414;
    case 172:  // __NR_prctl
      return 172;
    case 180:  // __NR_pread64
      return 180;
    case 333:  // __NR_preadv
      return 361;
    case 378:  // __NR_preadv2
      return 392;
    case 340:  // __NR_prlimit64
      return 369;
    case 440:  // __NR_process_madvise
      return 440;
    case 448:  // __NR_process_mrelease
      return 448;
    case 347:  // __NR_process_vm_readv
      return 376;
    case 348:  // __NR_process_vm_writev
      return 377;
    case 44:  // __NR_prof - missing on arm
      return -1;
    case 98:  // __NR_profil - missing on arm
      return -1;
    case 308:  // __NR_pselect6
      return 335;
    case 413:  // __NR_pselect6_time64
      return 413;
    case 26:  // __NR_ptrace
      return 26;
    case 189:  // __NR_putpmsg - missing on arm
      return -1;
    case 181:  // __NR_pwrite64
      return 181;
    case 334:  // __NR_pwritev
      return 362;
    case 379:  // __NR_pwritev2
      return 393;
    case 167:  // __NR_query_module - missing on arm
      return -1;
    case 131:  // __NR_quotactl
      return 131;
    case 443:  // __NR_quotactl_fd
      return 443;
    case 3:  // __NR_read
      return 3;
    case 225:  // __NR_readahead
      return 225;
    case 89:  // __NR_readdir - missing on arm
      return -1;
    case 85:  // __NR_readlink
      return 85;
    case 305:  // __NR_readlinkat
      return 332;
    case 145:  // __NR_readv
      return 145;
    case 88:  // __NR_reboot
      return 88;
    case 371:  // __NR_recvfrom
      return 292;
    case 337:  // __NR_recvmmsg
      return 365;
    case 417:  // __NR_recvmmsg_time64
      return 417;
    case 372:  // __NR_recvmsg
      return 297;
    case 257:  // __NR_remap_file_pages
      return 253;
    case 235:  // __NR_removexattr
      return 235;
    case 38:  // __NR_rename
      return 38;
    case 302:  // __NR_renameat
      return 329;
    case 353:  // __NR_renameat2
      return 382;
    case 287:  // __NR_request_key
      return 310;
    case 0:  // __NR_restart_syscall
      return 0;
    case 40:  // __NR_rmdir
      return 40;
    case 386:  // __NR_rseq
      return 398;
    case 174:  // __NR_rt_sigaction
      return 174;
    case 176:  // __NR_rt_sigpending
      return 176;
    case 175:  // __NR_rt_sigprocmask
      return 175;
    case 178:  // __NR_rt_sigqueueinfo
      return 178;
    case 173:  // __NR_rt_sigreturn
      return 173;
    case 179:  // __NR_rt_sigsuspend
      return 179;
    case 177:  // __NR_rt_sigtimedwait
      return 177;
    case 421:  // __NR_rt_sigtimedwait_time64
      return 421;
    case 335:  // __NR_rt_tgsigqueueinfo
      return 363;
    case 159:  // __NR_sched_get_priority_max
      return 159;
    case 160:  // __NR_sched_get_priority_min
      return 160;
    case 242:  // __NR_sched_getaffinity
      return 242;
    case 352:  // __NR_sched_getattr
      return 381;
    case 155:  // __NR_sched_getparam
      return 155;
    case 157:  // __NR_sched_getscheduler
      return 157;
    case 161:  // __NR_sched_rr_get_interval
      return 161;
    case 423:  // __NR_sched_rr_get_interval_time64
      return 423;
    case 241:  // __NR_sched_setaffinity
      return 241;
    case 351:  // __NR_sched_setattr
      return 380;
    case 154:  // __NR_sched_setparam
      return 154;
    case 156:  // __NR_sched_setscheduler
      return 156;
    case 158:  // __NR_sched_yield
      return 158;
    case 354:  // __NR_seccomp
      return 383;
    case 82:  // __NR_select - missing on arm
      return -1;
    case 394:  // __NR_semctl
      return 300;
    case 393:  // __NR_semget
      return 299;
    case 420:  // __NR_semtimedop_time64
      return 420;
    case 187:  // __NR_sendfile
      return 187;
    case 239:  // __NR_sendfile64
      return 239;
    case 345:  // __NR_sendmmsg
      return 374;
    case 370:  // __NR_sendmsg
      return 296;
    case 369:  // __NR_sendto
      return 290;
    case 276:  // __NR_set_mempolicy
      return 321;
    case 450:  // __NR_set_mempolicy_home_node
      return 450;
    case 311:  // __NR_set_robust_list
      return 338;
    case 243:  // __NR_set_thread_area - missing on arm
      return -1;
    case 258:  // __NR_set_tid_address
      return 256;
    case 121:  // __NR_setdomainname
      return 121;
    case 139:  // __NR_setfsgid
      return 139;
    case 216:  // __NR_setfsgid32
      return 216;
    case 138:  // __NR_setfsuid
      return 138;
    case 215:  // __NR_setfsuid32
      return 215;
    case 46:  // __NR_setgid
      return 46;
    case 214:  // __NR_setgid32
      return 214;
    case 81:  // __NR_setgroups
      return 81;
    case 206:  // __NR_setgroups32
      return 206;
    case 74:  // __NR_sethostname
      return 74;
    case 104:  // __NR_setitimer
      return 104;
    case 346:  // __NR_setns
      return 375;
    case 57:  // __NR_setpgid
      return 57;
    case 97:  // __NR_setpriority
      return 97;
    case 71:  // __NR_setregid
      return 71;
    case 204:  // __NR_setregid32
      return 204;
    case 170:  // __NR_setresgid
      return 170;
    case 210:  // __NR_setresgid32
      return 210;
    case 164:  // __NR_setresuid
      return 164;
    case 208:  // __NR_setresuid32
      return 208;
    case 70:  // __NR_setreuid
      return 70;
    case 203:  // __NR_setreuid32
      return 203;
    case 75:  // __NR_setrlimit
      return 75;
    case 66:  // __NR_setsid
      return 66;
    case 366:  // __NR_setsockopt
      return 294;
    case 79:  // __NR_settimeofday
      return 79;
    case 23:  // __NR_setuid
      return 23;
    case 213:  // __NR_setuid32
      return 213;
    case 226:  // __NR_setxattr
      return 226;
    case 68:  // __NR_sgetmask - missing on arm
      return -1;
    case 397:  // __NR_shmat
      return 305;
    case 396:  // __NR_shmctl
      return 308;
    case 398:  // __NR_shmdt
      return 306;
    case 395:  // __NR_shmget
      return 307;
    case 373:  // __NR_shutdown
      return 293;
    case 67:  // __NR_sigaction
      return 67;
    case 186:  // __NR_sigaltstack
      return 186;
    case 48:  // __NR_signal - missing on arm
      return -1;
    case 321:  // __NR_signalfd
      return 349;
    case 327:  // __NR_signalfd4
      return 355;
    case 73:  // __NR_sigpending
      return 73;
    case 126:  // __NR_sigprocmask
      return 126;
    case 119:  // __NR_sigreturn
      return 119;
    case 72:  // __NR_sigsuspend
      return 72;
    case 359:  // __NR_socket
      return 281;
    case 102:  // __NR_socketcall - missing on arm
      return -1;
    case 360:  // __NR_socketpair
      return 288;
    case 313:  // __NR_splice
      return 340;
    case 69:  // __NR_ssetmask - missing on arm
      return -1;
    case 106:  // __NR_stat
      return 106;
    case 195:  // __NR_stat64
      return 195;
    case 99:  // __NR_statfs
      return 99;
    case 268:  // __NR_statfs64
      return 266;
    case 383:  // __NR_statx
      return 397;
    case 25:  // __NR_stime - missing on arm
      return -1;
    case 31:  // __NR_stty - missing on arm
      return -1;
    case 115:  // __NR_swapoff
      return 115;
    case 87:  // __NR_swapon
      return 87;
    case 83:  // __NR_symlink
      return 83;
    case 304:  // __NR_symlinkat
      return 331;
    case 36:  // __NR_sync
      return 36;
    case 314:  // __NR_sync_file_range - missing on arm
      return -1;
    case 344:  // __NR_syncfs
      return 373;
    case 135:  // __NR_sysfs
      return 135;
    case 116:  // __NR_sysinfo
      return 116;
    case 103:  // __NR_syslog
      return 103;
    case 315:  // __NR_tee
      return 342;
    case 270:  // __NR_tgkill
      return 268;
    case 13:  // __NR_time - missing on arm
      return -1;
    case 259:  // __NR_timer_create
      return 257;
    case 263:  // __NR_timer_delete
      return 261;
    case 262:  // __NR_timer_getoverrun
      return 260;
    case 261:  // __NR_timer_gettime
      return 259;
    case 408:  // __NR_timer_gettime64
      return 408;
    case 260:  // __NR_timer_settime
      return 258;
    case 409:  // __NR_timer_settime64
      return 409;
    case 322:  // __NR_timerfd_create
      return 350;
    case 326:  // __NR_timerfd_gettime
      return 354;
    case 410:  // __NR_timerfd_gettime64
      return 410;
    case 325:  // __NR_timerfd_settime
      return 353;
    case 411:  // __NR_timerfd_settime64
      return 411;
    case 43:  // __NR_times
      return 43;
    case 238:  // __NR_tkill
      return 238;
    case 92:  // __NR_truncate
      return 92;
    case 193:  // __NR_truncate64
      return 193;
    case 191:  // __NR_ugetrlimit
      return 191;
    case 58:  // __NR_ulimit - missing on arm
      return -1;
    case 60:  // __NR_umask
      return 60;
    case 22:  // __NR_umount - missing on arm
      return -1;
    case 52:  // __NR_umount2
      return 52;
    case 122:  // __NR_uname
      return 122;
    case 10:  // __NR_unlink
      return 10;
    case 301:  // __NR_unlinkat
      return 328;
    case 310:  // __NR_unshare
      return 337;
    case 86:  // __NR_uselib
      return 86;
    case 374:  // __NR_userfaultfd
      return 388;
    case 62:  // __NR_ustat
      return 62;
    case 30:  // __NR_utime - missing on arm
      return -1;
    case 320:  // __NR_utimensat
      return 348;
    case 412:  // __NR_utimensat_time64
      return 412;
    case 271:  // __NR_utimes
      return 269;
    case 190:  // __NR_vfork
      return 190;
    case 111:  // __NR_vhangup
      return 111;
    case 166:  // __NR_vm86 - missing on arm
      return -1;
    case 113:  // __NR_vm86old - missing on arm
      return -1;
    case 316:  // __NR_vmsplice
      return 343;
    case 273:  // __NR_vserver
      return 313;
    case 114:  // __NR_wait4
      return 114;
    case 284:  // __NR_waitid
      return 280;
    case 7:  // __NR_waitpid - missing on arm
      return -1;
    case 4:  // __NR_write
      return 4;
    case 146:  // __NR_writev
      return 146;
    default:
      return -1;
  }
}

}  // namespace berberis
