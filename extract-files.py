#!/usr/bin/env -S PYTHONPATH=../../../tools/extract-utils python3
#
# SPDX-FileCopyrightText: 2024 The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

from extract_utils.file import File
from extract_utils.fixups_blob import (
    BlobFixupCtx,
    blob_fixup,
    blob_fixups_user_type,
)
from extract_utils.fixups_lib import (
    lib_fixups,
    lib_fixups_user_type,
)
from extract_utils.main import (
    ExtractUtils,
    ExtractUtilsModule,
)

namespace_imports = [
    'device/xiaomi/sm8550-common',
    'hardware/qcom-caf/wlan',
    'hardware/qcom-caf/sm8550',
    'hardware/xiaomi',
    'vendor/qcom/opensource/commonsys/display',
    'vendor/qcom/opensource/commonsys-intf/display',
    'vendor/qcom/opensource/dataservices',
]

def lib_fixup_vendor_suffix(lib: str, partition: str, *args, **kwargs):
    return f'{lib}-{partition}' if partition == 'vendor' else None


lib_fixups: lib_fixups_user_type = {
    **lib_fixups,
    (
        'vendor.qti.hardware.qccsyshal@1.0',
        'vendor.qti.hardware.qccsyshal@1.1',
        'vendor.qti.hardware.qccsyshal@1.2',
        'vendor.qti.hardware.qccvndhal@1.0',
        'vendor.qti.hardware.wifidisplaysession@1.0',
        'vendor.qti.imsrtpservice@3.0',
        'vendor.qti.imsrtpservice@3.1',
        'vendor.qti.diaghal@1.0',
    ): lib_fixup_vendor_suffix,
}

blob_fixups: blob_fixups_user_type = {
    'system/framework/WfdCommon.jar': blob_fixup()
        .apktool_patch('blob-patches/WfdCommon.patch'),
    'system_ext/lib64/libwfdservice.so': blob_fixup()
        .replace_needed('android.media.audio.common.types-V4-cpp.so', 'android.media.audio.common.types-V5-cpp.so'),
    'system_ext/lib64/vendor.qti.hardware.qccsyshal@1.2-halimpl.so': blob_fixup()
        .replace_needed('libprotobuf-cpp-full.so', 'libprotobuf-cpp-full-21.7.so'),
    'odm/lib64/libmt@1.3.so': blob_fixup()
        .replace_needed('libcrypto.so', 'libcrypto-v33.so'),
    (
        'vendor/bin/hw/android.hardware.security.keymint-service-qti',
        'vendor/lib64/libqtikeymint.so'
    ): blob_fixup()
        .add_needed('android.hardware.security.rkp-V3-ndk.so'),
    'vendor/etc/public.libraries.txt': blob_fixup()
        .regex_replace('libqti-perfd-client.so\n', ''),
    'vendor/etc/seccomp_policy/qwesd@2.0.policy': blob_fixup()
        .add_line_if_missing('pipe2: 1'),
    'vendor/lib64/libqcodec2_core.so': blob_fixup()
        .add_needed('libcodec2_shim.so'),
    'vendor/lib64/vendor.libdpmframework.so': blob_fixup()
        .add_needed('libhidlbase_shim.so'),
    (
        'vendor/bin/hw/android.hardware.contexthub-service.qmi',
        'vendor/lib64/libstfactory-vendor.so',
        'odm/lib64/nfc_nci.nqx.default.hw.so'
    'vendor/etc/media_codecs_kalama.xml': blob_fixup()
       'vendor/etc/media_codecs_kalama.xml',
    ): blob_fixup()
        # This is the file media_server actually parses, not media_codecs.xml.
        # init.qti.media.sh sets vendor.media.target_variant to _kalama on this
        # platform, init.qcom.rc mirrors that into ro.media.xml_variant.codecs,
        # and getDefaultXmlNames() then asks for "media_codecs" + variant + ".xml".
        # The sibling shipped in the image is media_codecs_performance_kalama.xml,
        # which confirms the variant is set.
        #
        # The google_*/vendor_audio includes stay stripped because those XML files
        # no longer exist as of Android 14 QPR3 and the swcodec apex already
        # provides those codecs.
        #
        # The two Dolby includes must be present. Without them AC-3, E-AC-3,
        # E-AC-3 JOC and AC-4 audio decode to silence and Dolby Vision recording
        # crashes MIUI Camera, because Codec2InfoBuilder drops any component that
        # is missing from the parsed codec map before it ever queries a store. The
        # decoders themselves are reached through IComponentStore/default1, and
        # the encoder through IComponentStore/dolby, both declared by
        # device/xiaomi/sm8550-common-dolby.
        .regex_replace('.+media_codecs_(google_audio|google_c2|google_telephony|vendor_audio).+\n', '')
        .regex_replace(
            r'(?m)^([ \t]*)<Include href="media_codecs_c2_audio\.xml"[ \t]*/>[ \t]*$',
            r'\1<Include href="media_codecs_c2_audio.xml" />\n'
            r'\1<Include href="media_codecs_dolby_audio.xml" />\n'
            r'\1<Include href="media_codecs_fuxi_dolby_vision.xml" />',
        ),
    'vendor/etc/vintf/manifest/c2_manifest_vendor.xml': blob_fixup()
        .regex_replace(r'\s*<fqname>@1\.0::IComponentStore/dolby</fqname>', '')
        .regex_replace('.+DOLBY.+\n', ''),
    (
        'vendor/lib64/libdpps.so',
        'vendor/lib64/libsnapdragoncolor-manager.so'
    ): blob_fixup()
        .replace_needed('libtinyxml2.so', 'libtinyxml2-v34.so'),
    'vendor/etc/init/hw/init.batterysecret.rc': blob_fixup()
        .regex_replace('group system system wakelock', 'group system system usb wakelock'),
    # kalama has no SLPI (sensors run on the ADSP); the write waits 60s and fails
    'vendor/etc/init/init.vendor.sensors.rc': blob_fixup()
        .regex_replace('.*boot_slpi.*\n', ''),
}  # fmt: skip

module = ExtractUtilsModule(
    'sm8550-common',
    'xiaomi',
    blob_fixups=blob_fixups,
    lib_fixups=lib_fixups,
    namespace_imports=namespace_imports,
)

if __name__ == '__main__':
    utils = ExtractUtils.device(module)
    utils.run()
