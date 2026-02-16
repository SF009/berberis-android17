/*
 * Copyright (C) 2015 The Android Open Source Project
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

// Unfortunately when GlibC is used attempt to include <sys/ioctl.h> brings in definitions of
// winsize and termio data structures - and then we couldn't include <asm/termio.h> to get kernel
// definitions of these data structures.
//
// Thus we include that header first - before anything else and with "special dance" to make it
// safe on GlibC.
//
// On Bionic it's safe to include it without additional precautions.
//
// First we need to include some default header to make sure we would see __GLIBC__ define.
#include <inttypes.h>
// Now let's rename GLibC versions to glibc_winsize/glibc_termio if we are using glibc.
#if defined(__GLIBC__) || defined(ANDROID_HOST_MUSL)
#define winsize glibc_winsize
#define termio glibc_termio
#include <sys/ioctl.h>
#undef winsize
#undef termio
#include <sys/socket.h>
#endif

#include "sys_ioctl_emulation.h"

#include <asm/termios.h>
#if __has_include(<linux/sync_file.h>)
#include <linux/sync_file.h>
#endif
#include <linux/unistd.h>
#include <linux/wireless.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

// We couldn't use <net/if.h> with GlibC thus we are including Linux headers directly.
//
// It works fine on Bionic as well - but since these are Linux headers they couldn't just be
// sorted alphabetically and included in that order:
//   <linux/if.h> uses "struct sockaddr" defined in <sys/socket.h>
//   <linux/hdlc/ioctl.h> uses "IFNAMSIZ" defined in <linux/if.h>
#include <linux/hdlc/ioctl.h>
#include <linux/if.h>
#include <linux/if_arp.h>

#include "berberis/base/logging.h"
#include "berberis/base/struct_check.h"  // CHECK_*_LAYOUT
#include "berberis/base/tracing.h"

namespace berberis {

namespace {

// This is used to process flexible array members (which don't officiablly exist in C++, but that
// are used in iw_encode_ext).
//
// Keep it here for now because it doesn't seem as if it's needed anywhere else.
#define CHECK_FIELD_OFFSET(type, field, offset)                              \
  static_assert(offsetof(type, field) * CHAR_BIT == offset,                  \
                "offset of `" #field "' field in " #type " must be " #offset \
                " because it's " #offset " on guest");

// Termios data structures.
CHECK_STRUCT_LAYOUT(termio, 144, 16);
CHECK_FIELD_LAYOUT(termio, c_iflag, 0, 16);
CHECK_FIELD_LAYOUT(termio, c_oflag, 16, 16);
CHECK_FIELD_LAYOUT(termio, c_cflag, 32, 16);
CHECK_FIELD_LAYOUT(termio, c_lflag, 48, 16);
CHECK_FIELD_LAYOUT(termio, c_line, 64, 8);
CHECK_FIELD_LAYOUT(termio, c_cc, 72, 64);

CHECK_STRUCT_LAYOUT(termios, 288, 32);
CHECK_FIELD_LAYOUT(termios, c_iflag, 0, 32);
CHECK_FIELD_LAYOUT(termios, c_oflag, 32, 32);
CHECK_FIELD_LAYOUT(termios, c_cflag, 64, 32);
CHECK_FIELD_LAYOUT(termios, c_lflag, 96, 32);
CHECK_FIELD_LAYOUT(termios, c_line, 128, 8);
CHECK_FIELD_LAYOUT(termios, c_cc, 136, 152);

CHECK_STRUCT_LAYOUT(winsize, 64, 16);
CHECK_FIELD_LAYOUT(winsize, ws_row, 0, 16);
CHECK_FIELD_LAYOUT(winsize, ws_col, 16, 16);
CHECK_FIELD_LAYOUT(winsize, ws_xpixel, 32, 16);
CHECK_FIELD_LAYOUT(winsize, ws_ypixel, 48, 16);

// Socket data structures.
CHECK_STRUCT_LAYOUT(sockaddr, 128, 16);
CHECK_FIELD_LAYOUT(sockaddr, sa_family, 0, 16);
CHECK_FIELD_LAYOUT(sockaddr, sa_data, 16, 112);

CHECK_STRUCT_LAYOUT(ifmap, 128, 32);
CHECK_FIELD_LAYOUT(ifmap, mem_start, 0, 32);
CHECK_FIELD_LAYOUT(ifmap, mem_end, 32, 32);
CHECK_FIELD_LAYOUT(ifmap, base_addr, 64, 16);
CHECK_FIELD_LAYOUT(ifmap, irq, 80, 8);
CHECK_FIELD_LAYOUT(ifmap, dma, 88, 8);
CHECK_FIELD_LAYOUT(ifmap, port, 96, 8);

CHECK_STRUCT_LAYOUT(raw_hdlc_proto, 32, 16);
CHECK_FIELD_LAYOUT(raw_hdlc_proto, encoding, 0, 16);
CHECK_FIELD_LAYOUT(raw_hdlc_proto, parity, 16, 16);

CHECK_STRUCT_LAYOUT(cisco_proto, 64, 32);
CHECK_FIELD_LAYOUT(cisco_proto, interval, 0, 32);
CHECK_FIELD_LAYOUT(cisco_proto, timeout, 32, 32);

CHECK_STRUCT_LAYOUT(fr_proto, 192, 32);
CHECK_FIELD_LAYOUT(fr_proto, t391, 0, 32);
CHECK_FIELD_LAYOUT(fr_proto, t392, 32, 32);
CHECK_FIELD_LAYOUT(fr_proto, n391, 64, 32);
CHECK_FIELD_LAYOUT(fr_proto, n392, 96, 32);
CHECK_FIELD_LAYOUT(fr_proto, n393, 128, 32);
CHECK_FIELD_LAYOUT(fr_proto, lmi, 160, 16);
CHECK_FIELD_LAYOUT(fr_proto, dce, 176, 16);

CHECK_STRUCT_LAYOUT(fr_proto_pvc, 32, 32);
CHECK_FIELD_LAYOUT(fr_proto_pvc, dlci, 0, 32);

CHECK_STRUCT_LAYOUT(fr_proto_pvc_info, 160, 32);
CHECK_FIELD_LAYOUT(fr_proto_pvc_info, dlci, 0, 32);
CHECK_FIELD_LAYOUT(fr_proto_pvc_info, master, 32, 128);

CHECK_STRUCT_LAYOUT(sync_serial_settings, 96, 32);
CHECK_FIELD_LAYOUT(sync_serial_settings, clock_rate, 0, 32);
CHECK_FIELD_LAYOUT(sync_serial_settings, clock_type, 32, 32);
CHECK_FIELD_LAYOUT(sync_serial_settings, loopback, 64, 16);

CHECK_STRUCT_LAYOUT(te1_settings, 128, 32);
CHECK_FIELD_LAYOUT(te1_settings, clock_rate, 0, 32);
CHECK_FIELD_LAYOUT(te1_settings, clock_type, 32, 32);
CHECK_FIELD_LAYOUT(te1_settings, loopback, 64, 16);
CHECK_FIELD_LAYOUT(te1_settings, slot_map, 96, 32);

CHECK_STRUCT_LAYOUT(if_settings, 96, 32);
CHECK_FIELD_LAYOUT(if_settings, type, 0, 32);
CHECK_FIELD_LAYOUT(if_settings, size, 32, 32);
CHECK_FIELD_LAYOUT(if_settings, ifs_ifsu, 64, 32);
CHECK_FIELD_LAYOUT(if_settings, ifs_ifsu.raw_hdlc, 64, 32);
CHECK_FIELD_LAYOUT(if_settings, ifs_ifsu.cisco, 64, 32);
CHECK_FIELD_LAYOUT(if_settings, ifs_ifsu.fr, 64, 32);
CHECK_FIELD_LAYOUT(if_settings, ifs_ifsu.fr_pvc, 64, 32);
CHECK_FIELD_LAYOUT(if_settings, ifs_ifsu.fr_pvc_info, 64, 32);
CHECK_FIELD_LAYOUT(if_settings, ifs_ifsu.sync, 64, 32);
CHECK_FIELD_LAYOUT(if_settings, ifs_ifsu.te1, 64, 32);

CHECK_STRUCT_LAYOUT(ifreq, 256, 32);
CHECK_FIELD_LAYOUT(ifreq, ifr_ifrn, 0, 128);
CHECK_FIELD_LAYOUT(ifreq, ifr_ifrn.ifrn_name, 0, 128);
CHECK_FIELD_LAYOUT(ifreq, ifr_ifru.ifru_addr, 128, 128);
CHECK_FIELD_LAYOUT(ifreq, ifr_ifru.ifru_dstaddr, 128, 128);
CHECK_FIELD_LAYOUT(ifreq, ifr_ifru.ifru_broadaddr, 128, 128);
CHECK_FIELD_LAYOUT(ifreq, ifr_ifru.ifru_netmask, 128, 128);
CHECK_FIELD_LAYOUT(ifreq, ifr_ifru.ifru_hwaddr, 128, 128);
CHECK_FIELD_LAYOUT(ifreq, ifr_ifru.ifru_flags, 128, 16);
CHECK_FIELD_LAYOUT(ifreq, ifr_ifru.ifru_ivalue, 128, 32);
CHECK_FIELD_LAYOUT(ifreq, ifr_ifru.ifru_mtu, 128, 32);
CHECK_FIELD_LAYOUT(ifreq, ifr_ifru.ifru_map, 128, 128);
CHECK_FIELD_LAYOUT(ifreq, ifr_ifru.ifru_slave, 128, 128);
CHECK_FIELD_LAYOUT(ifreq, ifr_ifru.ifru_newname, 128, 128);
CHECK_FIELD_LAYOUT(ifreq, ifr_ifru.ifru_data, 128, 32);
CHECK_FIELD_LAYOUT(ifreq, ifr_ifru.ifru_settings, 128, 96);

CHECK_STRUCT_LAYOUT(ifconf, 64, 32);
CHECK_FIELD_LAYOUT(ifconf, ifc_len, 0, 32);
CHECK_FIELD_LAYOUT(ifconf, ifc_ifcu, 32, 32);
CHECK_FIELD_LAYOUT(ifconf, ifc_ifcu.ifcu_buf, 32, 32);
CHECK_FIELD_LAYOUT(ifconf, ifc_ifcu.ifcu_req, 32, 32);

CHECK_STRUCT_LAYOUT(arpreq, 544, 32);
CHECK_FIELD_LAYOUT(arpreq, arp_pa, 0, 128);
CHECK_FIELD_LAYOUT(arpreq, arp_ha, 128, 128);
CHECK_FIELD_LAYOUT(arpreq, arp_flags, 256, 32);
CHECK_FIELD_LAYOUT(arpreq, arp_netmask, 288, 128);
CHECK_FIELD_LAYOUT(arpreq, arp_dev, 416, 128);

// wireless data structures.
CHECK_STRUCT_LAYOUT(iw_param, 64, 32);
CHECK_FIELD_LAYOUT(iw_param, value, 0, 32);
CHECK_FIELD_LAYOUT(iw_param, fixed, 32, 8);
CHECK_FIELD_LAYOUT(iw_param, disabled, 40, 8);
CHECK_FIELD_LAYOUT(iw_param, flags, 48, 16);

CHECK_STRUCT_LAYOUT(iw_point, 64, 32);
CHECK_FIELD_LAYOUT(iw_point, pointer, 0, 32);
CHECK_FIELD_LAYOUT(iw_point, length, 32, 16);
CHECK_FIELD_LAYOUT(iw_point, flags, 48, 16);

CHECK_STRUCT_LAYOUT(iw_freq, 64, 32);
CHECK_FIELD_LAYOUT(iw_freq, m, 0, 32);
CHECK_FIELD_LAYOUT(iw_freq, e, 32, 16);
CHECK_FIELD_LAYOUT(iw_freq, i, 48, 8);
CHECK_FIELD_LAYOUT(iw_freq, flags, 56, 8);

CHECK_STRUCT_LAYOUT(iw_quality, 32, 8);
CHECK_FIELD_LAYOUT(iw_quality, qual, 0, 8);
CHECK_FIELD_LAYOUT(iw_quality, level, 8, 8);
CHECK_FIELD_LAYOUT(iw_quality, noise, 16, 8);
CHECK_FIELD_LAYOUT(iw_quality, updated, 24, 8);

CHECK_STRUCT_LAYOUT(iw_discarded, 160, 32);
CHECK_FIELD_LAYOUT(iw_discarded, nwid, 0, 32);
CHECK_FIELD_LAYOUT(iw_discarded, code, 32, 32);
CHECK_FIELD_LAYOUT(iw_discarded, fragment, 64, 32);
CHECK_FIELD_LAYOUT(iw_discarded, retries, 96, 32);
CHECK_FIELD_LAYOUT(iw_discarded, misc, 128, 32);

CHECK_STRUCT_LAYOUT(iw_missed, 32, 32);
CHECK_FIELD_LAYOUT(iw_missed, beacon, 0, 32);

CHECK_STRUCT_LAYOUT(iw_thrspy, 224, 16);
CHECK_FIELD_LAYOUT(iw_thrspy, addr, 0, 128);
CHECK_FIELD_LAYOUT(iw_thrspy, qual, 128, 32);
CHECK_FIELD_LAYOUT(iw_thrspy, low, 160, 32);
CHECK_FIELD_LAYOUT(iw_thrspy, high, 192, 32);

CHECK_STRUCT_LAYOUT(iw_scan_req, 2528, 32);
CHECK_FIELD_LAYOUT(iw_scan_req, scan_type, 0, 8);
CHECK_FIELD_LAYOUT(iw_scan_req, essid_len, 8, 8);
CHECK_FIELD_LAYOUT(iw_scan_req, num_channels, 16, 8);
CHECK_FIELD_LAYOUT(iw_scan_req, flags, 24, 8);
CHECK_FIELD_LAYOUT(iw_scan_req, bssid, 32, 128);
CHECK_FIELD_LAYOUT(iw_scan_req, essid, 160, 256);
CHECK_FIELD_LAYOUT(iw_scan_req, min_channel_time, 416, 32);
CHECK_FIELD_LAYOUT(iw_scan_req, max_channel_time, 448, 32);
CHECK_FIELD_LAYOUT(iw_scan_req, channel_list, 480, 2048);

CHECK_STRUCT_LAYOUT(iw_encode_ext, 320, 32);
CHECK_FIELD_LAYOUT(iw_encode_ext, ext_flags, 0, 32);
CHECK_FIELD_LAYOUT(iw_encode_ext, tx_seq, 32, 64);
CHECK_FIELD_LAYOUT(iw_encode_ext, rx_seq, 96, 64);
CHECK_FIELD_LAYOUT(iw_encode_ext, addr, 160, 128);
CHECK_FIELD_LAYOUT(iw_encode_ext, alg, 288, 16);
CHECK_FIELD_LAYOUT(iw_encode_ext, key_len, 304, 16);
CHECK_FIELD_OFFSET(iw_encode_ext, key, 320);

CHECK_STRUCT_LAYOUT(iw_mlme, 160, 16);
CHECK_FIELD_LAYOUT(iw_mlme, cmd, 0, 16);
CHECK_FIELD_LAYOUT(iw_mlme, reason_code, 16, 16);
CHECK_FIELD_LAYOUT(iw_mlme, addr, 32, 128);

CHECK_STRUCT_LAYOUT(iw_pmksa, 288, 32);
CHECK_FIELD_LAYOUT(iw_pmksa, cmd, 0, 32);
CHECK_FIELD_LAYOUT(iw_pmksa, bssid, 32, 128);
CHECK_FIELD_LAYOUT(iw_pmksa, pmkid, 160, 128);

CHECK_STRUCT_LAYOUT(iw_michaelmicfailure, 224, 32);
CHECK_FIELD_LAYOUT(iw_michaelmicfailure, flags, 0, 32);
CHECK_FIELD_LAYOUT(iw_michaelmicfailure, src_addr, 32, 128);
CHECK_FIELD_LAYOUT(iw_michaelmicfailure, tsc, 160, 64);

CHECK_STRUCT_LAYOUT(iw_pmkid_cand, 192, 32);
CHECK_FIELD_LAYOUT(iw_pmkid_cand, flags, 0, 32);
CHECK_FIELD_LAYOUT(iw_pmkid_cand, index, 32, 32);
CHECK_FIELD_LAYOUT(iw_pmkid_cand, bssid, 64, 128);

CHECK_STRUCT_LAYOUT(iw_statistics, 256, 32);
CHECK_FIELD_LAYOUT(iw_statistics, status, 0, 16);
CHECK_FIELD_LAYOUT(iw_statistics, qual, 16, 32);
CHECK_FIELD_LAYOUT(iw_statistics, discard, 64, 160);
CHECK_FIELD_LAYOUT(iw_statistics, miss, 224, 32);

CHECK_STRUCT_LAYOUT(iwreq_data, 128, 32);
CHECK_FIELD_LAYOUT(iwreq_data, name, 0, 128);
CHECK_FIELD_LAYOUT(iwreq_data, essid, 0, 64);
CHECK_FIELD_LAYOUT(iwreq_data, nwid, 0, 64);
CHECK_FIELD_LAYOUT(iwreq_data, freq, 0, 64);
CHECK_FIELD_LAYOUT(iwreq_data, sens, 0, 64);
CHECK_FIELD_LAYOUT(iwreq_data, bitrate, 0, 64);
CHECK_FIELD_LAYOUT(iwreq_data, txpower, 0, 64);
CHECK_FIELD_LAYOUT(iwreq_data, rts, 0, 64);
CHECK_FIELD_LAYOUT(iwreq_data, frag, 0, 64);
CHECK_FIELD_LAYOUT(iwreq_data, mode, 0, 32);
CHECK_FIELD_LAYOUT(iwreq_data, retry, 0, 64);
CHECK_FIELD_LAYOUT(iwreq_data, encoding, 0, 64);
CHECK_FIELD_LAYOUT(iwreq_data, power, 0, 64);
CHECK_FIELD_LAYOUT(iwreq_data, qual, 0, 32);
CHECK_FIELD_LAYOUT(iwreq_data, ap_addr, 0, 128);
CHECK_FIELD_LAYOUT(iwreq_data, addr, 0, 128);
CHECK_FIELD_LAYOUT(iwreq_data, param, 0, 64);
CHECK_FIELD_LAYOUT(iwreq_data, data, 0, 64);

CHECK_STRUCT_LAYOUT(iwreq, 256, 32);
CHECK_FIELD_LAYOUT(iwreq, ifr_ifrn.ifrn_name, 0, 128);

CHECK_STRUCT_LAYOUT(iw_range, 4544, 32);
CHECK_FIELD_LAYOUT(iw_range, throughput, 0, 32);
CHECK_FIELD_LAYOUT(iw_range, min_nwid, 32, 32);
CHECK_FIELD_LAYOUT(iw_range, max_nwid, 64, 32);
CHECK_FIELD_LAYOUT(iw_range, old_num_channels, 96, 16);
CHECK_FIELD_LAYOUT(iw_range, old_num_frequency, 112, 8);
CHECK_FIELD_LAYOUT(iw_range, scan_capa, 120, 8);
CHECK_FIELD_LAYOUT(iw_range, event_capa, 128, 192);
CHECK_FIELD_LAYOUT(iw_range, sensitivity, 320, 32);
CHECK_FIELD_LAYOUT(iw_range, max_qual, 352, 32);
CHECK_FIELD_LAYOUT(iw_range, avg_qual, 384, 32);
CHECK_FIELD_LAYOUT(iw_range, num_bitrates, 416, 8);
CHECK_FIELD_LAYOUT(iw_range, bitrate, 448, 1024);
CHECK_FIELD_LAYOUT(iw_range, min_rts, 1472, 32);
CHECK_FIELD_LAYOUT(iw_range, max_rts, 1504, 32);
CHECK_FIELD_LAYOUT(iw_range, min_frag, 1536, 32);
CHECK_FIELD_LAYOUT(iw_range, max_frag, 1568, 32);
CHECK_FIELD_LAYOUT(iw_range, min_pmp, 1600, 32);
CHECK_FIELD_LAYOUT(iw_range, max_pmp, 1632, 32);
CHECK_FIELD_LAYOUT(iw_range, min_pmt, 1664, 32);
CHECK_FIELD_LAYOUT(iw_range, max_pmt, 1696, 32);
CHECK_FIELD_LAYOUT(iw_range, pmp_flags, 1728, 16);
CHECK_FIELD_LAYOUT(iw_range, pmt_flags, 1744, 16);
CHECK_FIELD_LAYOUT(iw_range, pm_capa, 1760, 16);
CHECK_FIELD_LAYOUT(iw_range, encoding_size, 1776, 128);
CHECK_FIELD_LAYOUT(iw_range, num_encoding_sizes, 1904, 8);
CHECK_FIELD_LAYOUT(iw_range, max_encoding_tokens, 1912, 8);
CHECK_FIELD_LAYOUT(iw_range, encoding_login_index, 1920, 8);
CHECK_FIELD_LAYOUT(iw_range, txpower_capa, 1936, 16);
CHECK_FIELD_LAYOUT(iw_range, num_txpower, 1952, 8);
CHECK_FIELD_LAYOUT(iw_range, txpower, 1984, 256);
CHECK_FIELD_LAYOUT(iw_range, we_version_compiled, 2240, 8);
CHECK_FIELD_LAYOUT(iw_range, we_version_source, 2248, 8);
CHECK_FIELD_LAYOUT(iw_range, retry_capa, 2256, 16);
CHECK_FIELD_LAYOUT(iw_range, retry_flags, 2272, 16);
CHECK_FIELD_LAYOUT(iw_range, r_time_flags, 2288, 16);
CHECK_FIELD_LAYOUT(iw_range, min_retry, 2304, 32);
CHECK_FIELD_LAYOUT(iw_range, max_retry, 2336, 32);
CHECK_FIELD_LAYOUT(iw_range, min_r_time, 2368, 32);
CHECK_FIELD_LAYOUT(iw_range, max_r_time, 2400, 32);
CHECK_FIELD_LAYOUT(iw_range, num_channels, 2432, 16);
CHECK_FIELD_LAYOUT(iw_range, num_frequency, 2448, 8);
CHECK_FIELD_LAYOUT(iw_range, freq, 2464, 2048);
CHECK_FIELD_LAYOUT(iw_range, enc_capa, 4512, 32);

CHECK_STRUCT_LAYOUT(iw_priv_args, 192, 32);
CHECK_FIELD_LAYOUT(iw_priv_args, cmd, 0, 32);
CHECK_FIELD_LAYOUT(iw_priv_args, set_args, 32, 16);
CHECK_FIELD_LAYOUT(iw_priv_args, get_args, 48, 16);
CHECK_FIELD_LAYOUT(iw_priv_args, name, 64, 128);

CHECK_STRUCT_LAYOUT(iw_event, 160, 32);
CHECK_FIELD_LAYOUT(iw_event, len, 0, 16);
CHECK_FIELD_LAYOUT(iw_event, cmd, 16, 16);
CHECK_FIELD_LAYOUT(iw_event, u, 32, 128);

// libsync data structures. Note: some of these are NOT available on master because they represent
// an obsolete interface... but they are used on versions of Android with obsolete kernels...
#if __has_include(<linux/sync_file.h>)

struct sync_legacy_merge_data {
  int32_t fd2;
  char name[32];
  int32_t fence;
};

#define SYNC_IOC_LEGACY_MERGE _IOWR(SYNC_IOC_MAGIC, 1, struct sync_legacy_merge_data)
#define SYNC_IOC_LEGACY_FENCE_INFO _IOWR(SYNC_IOC_MAGIC, 2, struct sync_fence_info)

struct sw_sync_create_fence_data {
  __u32 value;
  char name[32];
  __s32 fence;
};

#define SW_SYNC_IOC_MAGIC 'W'
#define SW_SYNC_IOC_CREATE_FENCE _IOWR(SW_SYNC_IOC_MAGIC, 0, struct sw_sync_create_fence_data)
#define SW_SYNC_IOC_INC _IOW(SW_SYNC_IOC_MAGIC, 1, __u32)

CHECK_STRUCT_LAYOUT(sw_sync_create_fence_data, 320, 32);
CHECK_FIELD_LAYOUT(sw_sync_create_fence_data, value, 0, 32);
CHECK_FIELD_LAYOUT(sw_sync_create_fence_data, name, 32, 256);
CHECK_FIELD_LAYOUT(sw_sync_create_fence_data, fence, 288, 32);

#ifdef __i386__
// Guest sync_fence_info is more aligned than host sync_fence_info - but offsets and sizes of all
// fields are equal, so guest sync_fence_info can be used in place of host sync_fence_info.
CHECK_STRUCT_LAYOUT(sync_fence_info, 640, 32);
#else
CHECK_STRUCT_LAYOUT(sync_fence_info, 640, 64);
#endif
CHECK_FIELD_LAYOUT(sync_fence_info, obj_name, 0, 256);
CHECK_FIELD_LAYOUT(sync_fence_info, driver_name, 256, 256);
CHECK_FIELD_LAYOUT(sync_fence_info, status, 512, 32);
CHECK_FIELD_LAYOUT(sync_fence_info, flags, 544, 32);
CHECK_FIELD_LAYOUT(sync_fence_info, timestamp_ns, 576, 64);

#ifdef __i386__
// Guest sync_file_info is more aligned than host sync_file_info - but offsets and sizes of all
// fields are equal, so guest sync_file_info can be used in place of host sync_file_info.
CHECK_STRUCT_LAYOUT(sync_file_info, 448, 32);
#else
CHECK_STRUCT_LAYOUT(sync_file_info, 448, 64);
#endif
CHECK_FIELD_LAYOUT(sync_file_info, name, 0, 256);
CHECK_FIELD_LAYOUT(sync_file_info, status, 256, 32);
CHECK_FIELD_LAYOUT(sync_file_info, flags, 288, 32);
CHECK_FIELD_LAYOUT(sync_file_info, num_fences, 320, 32);
CHECK_FIELD_LAYOUT(sync_file_info, pad, 352, 32);
CHECK_FIELD_LAYOUT(sync_file_info, sync_fence_info, 384, 64);

CHECK_STRUCT_LAYOUT(sync_legacy_merge_data, 320, 32);
CHECK_FIELD_LAYOUT(sync_legacy_merge_data, fd2, 0, 32);
CHECK_FIELD_LAYOUT(sync_legacy_merge_data, name, 32, 256);
CHECK_FIELD_LAYOUT(sync_legacy_merge_data, fence, 288, 32);

CHECK_STRUCT_LAYOUT(sync_merge_data, 384, 32);
CHECK_FIELD_LAYOUT(sync_merge_data, name, 0, 256);
CHECK_FIELD_LAYOUT(sync_merge_data, fd2, 256, 32);
CHECK_FIELD_LAYOUT(sync_merge_data, fence, 288, 32);
CHECK_FIELD_LAYOUT(sync_merge_data, flags, 320, 32);
CHECK_FIELD_LAYOUT(sync_merge_data, pad, 352, 32);

#endif  // __has_include(<linux/sync_file.h>)

}  // namespace

int IoCtlForGuest(long d, long request, long arg) {
  switch (static_cast<unsigned long>(request)) {
    case SIOCADDMULTI:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCDARP:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCDELMULTI:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCDRARP:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCGARP:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCGIFINDEX:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCGIFADDR:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCGIFBRDADDR:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCGIFDSTADDR:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCGIFENCAP:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCGIFFLAGS:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCGIFHWADDR:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCGIFMAP:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCGIFMEM:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCGIFMETRIC:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCGIFMTU:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCGIFNAME:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCGIFNETMASK:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCGIFSLAVE:
      return syscall(__NR_ioctl, d, request);
    case SIOCGIWAPLIST:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCGIWAP:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCGIWAUTH:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCGIWENCODEEXT:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCGIWENCODE:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCGIWESSID:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCGIWFRAG:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCGIWFREQ:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCGIWGENIE:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCGIWMODE:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCGIWNAME:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCGIWNICKN:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCGIWNWID:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCGIWPOWER:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCGIWPRIV:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCGIWRANGE:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCGIWRATE:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCGIWRETRY:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCGIWRTS:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCGIWSCAN:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCGIWSENS:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCGIWSPY:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCGIWSTATS:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCGIWTHRSPY:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCGIWTXPOW:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCGRARP:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCSARP:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCSIFADDR:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCSIFBRDADDR:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCSIFDSTADDR:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCSIFENCAP:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCSIFFLAGS:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCSIFHWADDR:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCSIFLINK:
      return syscall(__NR_ioctl, d, request);
    case SIOCSIFMAP:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCSIFMEM:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCSIFMETRIC:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCSIFMTU:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCSIFNAME:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCSIFNETMASK:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCSIFSLAVE:
      return syscall(__NR_ioctl, d, request);
    case SIOCSRARP:
      return syscall(__NR_ioctl, d, request, arg);
#if __has_include(<linux/sync_file.h>)
    case SIOCSIWAP:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCSIWAUTH:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCSIWCOMMIT:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCSIWENCODEEXT:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCSIWENCODE:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCSIWESSID:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCSIWFRAG:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCSIWFREQ:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCSIWGENIE:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCSIWMLME:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCSIWMODE:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCSIWNICKN:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCSIWNWID:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCSIWPMKSA:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCSIWPOWER:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCSIWPRIV:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCSIWRANGE:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCSIWRATE:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCSIWRETRY:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCSIWRTS:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCSIWSCAN:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCSIWSENS:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCSIWSPY:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCSIWSTATS:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCSIWTHRSPY:
      return syscall(__NR_ioctl, d, request, arg);
    case SIOCSIWTXPOW:
      return syscall(__NR_ioctl, d, request, arg);
    case SW_SYNC_IOC_CREATE_FENCE:
      return syscall(__NR_ioctl, d, request, arg);
    case SYNC_IOC_FILE_INFO:
      return syscall(__NR_ioctl, d, request, arg);
    case SW_SYNC_IOC_INC:
      return syscall(__NR_ioctl, d, request, arg);
    case SYNC_IOC_LEGACY_FENCE_INFO:
      return syscall(__NR_ioctl, d, request, arg);
    case SYNC_IOC_LEGACY_MERGE:
      return syscall(__NR_ioctl, d, request, arg);
    case SYNC_IOC_MERGE:
      return syscall(__NR_ioctl, d, request, arg);
#endif
    case TCGETA:
      return syscall(__NR_ioctl, d, request, arg);
    case TCGETS:
      return syscall(__NR_ioctl, d, request, arg);
    case TCSETA:
      return syscall(__NR_ioctl, d, request, arg);
    case TCSETAF:
      return syscall(__NR_ioctl, d, request, arg);
    case TCSETAW:
      return syscall(__NR_ioctl, d, request, arg);
    case TCSETS:
      return syscall(__NR_ioctl, d, request, arg);
    case TCSETSF:
      return syscall(__NR_ioctl, d, request, arg);
    case TCSETSW:
      return syscall(__NR_ioctl, d, request, arg);
    case TIOCGPTN:
      return syscall(__NR_ioctl, d, request, arg);
    case TIOCGWINSZ:
      return syscall(__NR_ioctl, d, request, arg);
    case TIOCSPTLCK:
      return syscall(__NR_ioctl, d, request, arg);
    case TIOCSCTTY:
      return syscall(__NR_ioctl, d, request, arg);
    case TIOCSWINSZ:
      return syscall(__NR_ioctl, d, request, arg);
    default: {
      TRACE("unimplemented ioctl 0x%lx, running host syscall as is", request);
      return syscall(__NR_ioctl, d, request, arg);
    }
  }
}

}  // namespace berberis
