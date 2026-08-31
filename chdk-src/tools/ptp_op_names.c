#include <stddef.h>
#include <stdint.h>
#include "ptp_op_names.h"

/*
from https://github.com/gphoto/libgphoto2/blob/master/camlibs/ptp2/ptp.h
https://github.com/coon42/magiclantern_simplified/blob/ptpcam_add_more_canon_codes/contrib/ptpcam/ptp.h
and https://github.com/lclevy/miniPtp/blob/main/ptp.yml
as of 2023/12/24
sources are identified with ### Magic Lantern, ### Gphoto or ### miniPtp

Defines here are copied to minimize update effort
Magic Lantern names for codes > 0x9100 have EOS_ added to align with Gphoto naming
*/
/* This code is based on gphoto
 *
 * Copyright (C) 2001 Mariusz Woloszyn <emsi@ipartners.pl>
 * Copyright (C) 2003-2020 Marcus Meissner <marcus@jet.franken.de>
 * Copyright (C) 2006-2008 Linus Walleij <triad@df.lth.se>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

// ### Gphoto
/* PTP v1.0 operation codes */
#define PTP_OC_GetDeviceInfo            0x1001
#define PTP_OC_OpenSession              0x1002
#define PTP_OC_CloseSession             0x1003
#define PTP_OC_GetStorageIDs            0x1004
#define PTP_OC_GetStorageInfo           0x1005
#define PTP_OC_GetNumObjects            0x1006
#define PTP_OC_GetObjectHandles         0x1007
#define PTP_OC_GetObjectInfo            0x1008
#define PTP_OC_GetObject                0x1009
#define PTP_OC_GetThumb                 0x100A
#define PTP_OC_DeleteObject             0x100B
#define PTP_OC_SendObjectInfo           0x100C
#define PTP_OC_SendObject               0x100D
#define PTP_OC_InitiateCapture          0x100E
#define PTP_OC_FormatStore              0x100F
#define PTP_OC_ResetDevice              0x1010
#define PTP_OC_SelfTest                 0x1011
#define PTP_OC_SetObjectProtection      0x1012
#define PTP_OC_PowerDown                0x1013
#define PTP_OC_GetDevicePropDesc        0x1014
#define PTP_OC_GetDevicePropValue       0x1015
#define PTP_OC_SetDevicePropValue       0x1016
#define PTP_OC_ResetDevicePropValue     0x1017
#define PTP_OC_TerminateOpenCapture     0x1018
#define PTP_OC_MoveObject               0x1019
#define PTP_OC_CopyObject               0x101A
#define PTP_OC_GetPartialObject         0x101B
#define PTP_OC_InitiateOpenCapture      0x101C
/* PTP v1.1 operation codes */
#define PTP_OC_StartEnumHandles		0x101D
#define PTP_OC_EnumHandles		0x101E
#define PTP_OC_StopEnumHandles		0x101F
#define PTP_OC_GetVendorExtensionMaps	0x1020
#define PTP_OC_GetVendorDeviceInfo	0x1021
#define PTP_OC_GetResizedImageObject	0x1022
#define PTP_OC_GetFilesystemManifest	0x1023
#define PTP_OC_GetStreamInfo		0x1024
#define PTP_OC_GetStream		0x1025

/* Canon extension Operation Codes */
#define PTP_OC_CANON_GetPartialObjectInfo	0x9001
/* 9002 - sends 2 uint32, nothing back  */
#define PTP_OC_CANON_SetObjectArchive		0x9002
#define PTP_OC_CANON_KeepDeviceOn		0x9003
#define PTP_OC_CANON_LockDeviceUI		0x9004
#define PTP_OC_CANON_UnlockDeviceUI		0x9005
#define PTP_OC_CANON_GetObjectHandleByName	0x9006
/* no 9007 observed yet */
#define PTP_OC_CANON_InitiateReleaseControl	0x9008
#define PTP_OC_CANON_TerminateReleaseControl	0x9009
#define PTP_OC_CANON_TerminatePlaybackMode	0x900A
#define PTP_OC_CANON_ViewfinderOn		0x900B
#define PTP_OC_CANON_ViewfinderOff		0x900C
#define PTP_OC_CANON_DoAeAfAwb			0x900D

/* 900e - send nothing, gets 5 uint16t in 32bit entities back in 20byte datablob */
#define PTP_OC_CANON_GetCustomizeSpec		0x900E
#define PTP_OC_CANON_GetCustomizeItemInfo	0x900F
#define PTP_OC_CANON_GetCustomizeData		0x9010
#define PTP_OC_CANON_SetCustomizeData		0x9011
#define PTP_OC_CANON_GetCaptureStatus		0x9012
#define PTP_OC_CANON_CheckEvent			0x9013
#define PTP_OC_CANON_FocusLock			0x9014
#define PTP_OC_CANON_FocusUnlock		0x9015
#define PTP_OC_CANON_GetLocalReleaseParam	0x9016
#define PTP_OC_CANON_SetLocalReleaseParam	0x9017
#define PTP_OC_CANON_AskAboutPcEvf		0x9018
#define PTP_OC_CANON_SendPartialObject		0x9019
#define PTP_OC_CANON_InitiateCaptureInMemory	0x901A
#define PTP_OC_CANON_GetPartialObjectEx		0x901B
#define PTP_OC_CANON_SetObjectTime		0x901C
#define PTP_OC_CANON_GetViewfinderImage		0x901D
#define PTP_OC_CANON_GetObjectAttributes	0x901E
#define PTP_OC_CANON_ChangeUSBProtocol		0x901F
#define PTP_OC_CANON_GetChanges			0x9020
#define PTP_OC_CANON_GetObjectInfoEx		0x9021
#define PTP_OC_CANON_InitiateDirectTransfer	0x9022
#define PTP_OC_CANON_TerminateDirectTransfer 	0x9023
#define PTP_OC_CANON_SendObjectInfoByPath 	0x9024
#define PTP_OC_CANON_SendObjectByPath 		0x9025
#define PTP_OC_CANON_InitiateDirectTansferEx	0x9026
#define PTP_OC_CANON_GetAncillaryObjectHandles	0x9027
#define PTP_OC_CANON_GetTreeInfo 		0x9028
#define PTP_OC_CANON_GetTreeSize 		0x9029
#define PTP_OC_CANON_NotifyProgress 		0x902A
#define PTP_OC_CANON_NotifyCancelAccepted	0x902B
/* 902c: no parms, read 3 uint32 in data, no response parms */
//#define PTP_OC_CANON_902C			0x902C
#define PTP_OC_CANON_GetDirectory		0x902D
//#define PTP_OC_CANON_902E			0x902E

// ### miniPtp
#define PTP_OC_CANON_PingRequest			0x902F	/* used during camera init */

// ### Gphoto
#define PTP_OC_CANON_SetPairingInfo		0x9030
#define PTP_OC_CANON_GetPairingInfo		0x9031
#define PTP_OC_CANON_DeletePairingInfo		0x9032
#define PTP_OC_CANON_GetMACAddress		0x9033 /* no args */
/*
0000  12 00 00 00 02 00 33 90-1a 00 00 00 2c 9e fc c8  ......3.....,...
0010  33 e3                  -                         3.
 */

/* 9034: 1 param, no parms returned */
#define PTP_OC_CANON_SetDisplayMonitor		0x9034
#define PTP_OC_CANON_PairingComplete		0x9035
#define PTP_OC_CANON_GetWirelessMAXChannel	0x9036

// ### Magic Lantern
#define PTP_OC_CANON_InitiateEventProc0 0x9050 // https://chdk.setepontos.com/index.php?topic=4338.msg147738#msg147738
#define PTP_OC_CANON_TerminateEventProc_051 0x9051
#define PTP_OC_CANON_ExecuteEventProc 0x9052
#define PTP_OC_CANON_GetEventProcReturnData 0x9053
#define PTP_OC_CANON_IsEventProcRunning 0x9057
#define PTP_OC_CANON_QuerySizeOfTransparentMemory 0x9058
#define PTP_OC_CANON_LoadTransparentMemory 0x9059
#define PTP_OC_CANON_SaveTransparentMemory 0x905A
#define PTP_OC_CANON_QuickLoadTransparentMemory 0x905B
#define PTP_OC_CANON_InitiateEventProc1 0x905C
#define PTP_OC_CANON_TerminateEventProc_05D 0x905D

// ### Gphoto
#define PTP_OC_CANON_GetWebServiceSpec		0x9068 /* no args */
/* data returned:
0000  1e 00 00 00 02 00 68 90-1a 00 00 00 00 01 08 00  ......h.........
0010  14 00 bc ce 00 00 78 00-78 00 00 14 00 00        ......x.x.....
*/
#define PTP_OC_CANON_IsNeoKabotanProcMode 0x9060
#define PTP_OC_CANON_GetWebServiceData		0x9069 /* no args */
#define PTP_OC_CANON_SetWebServiceData		0x906A
#define PTP_OC_CANON_DeleteWebServiceData	0x906B
#define PTP_OC_CANON_GetRootCertificateSpec	0x906C /* no args */
/*
0000  12 00 00 00 02 00 6c 90-1a 00 00 00 00 01 6c 30  ......l.......l0
0010  00 00                  -                         ..
*/
#define PTP_OC_CANON_GetRootCertificateData	0x906D /* no args */
#define PTP_OC_CANON_SetRootCertificateData	0x906E
#define PTP_OC_CANON_DeleteRootCertificateData	0x906F
#define PTP_OC_CANON_GetGpsMobilelinkObjectInfo	0x9075 /* 2 args: utcstart, utcend */
#define PTP_OC_CANON_SendGpsTagInfo		0x9076 /* 1 arg: oid */
#define PTP_OC_CANON_GetTranscodeApproxSize	0x9077 /* 1 arg: oid? */
#define PTP_OC_CANON_RequestTranscodeStart	0x9078 /* 1 arg: oid? */
#define PTP_OC_CANON_RequestTranscodeCancel	0x9079 /* 1 arg: oid? */

#define PTP_OC_CANON_SetRemoteShootingMode	0x9086

// ### miniPtp
#define PTP_OC_CANON_GetDebugLog	0x9087
#define PTP_OC_CANON_SetModeDialDisable	0x9088

// ### Gphoto
/* 9101: no args, 8 byte data (01 00 00 00 00 00 00 00), no resp data. */
#define PTP_OC_CANON_EOS_GetStorageIDs		0x9101
/* 9102: 1 arg (0)
 * 0x28 bytes of data:
    00000000: 34 00 00 00 02 00 02 91 0a 00 00 00 04 00 03 00
    00000010: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
    00000020: 00 00 ff ff ff ff 03 43 00 46 00 00 00 03 41 00
    00000030: 3a 00 00 00
 * no resp args
 */
#define PTP_OC_CANON_EOS_GetStorageInfo		0x9102
#define PTP_OC_CANON_EOS_GetObjectInfo		0x9103
#define PTP_OC_CANON_EOS_GetObject		0x9104
#define PTP_OC_CANON_EOS_DeleteObject		0x9105
#define PTP_OC_CANON_EOS_FormatStore		0x9106
#define PTP_OC_CANON_EOS_GetPartialObject	0x9107
#define PTP_OC_CANON_EOS_GetDeviceInfoEx	0x9108

/* sample1:
 * 3 cmdargs: 1,0xffffffff,00 00 10 00;
 * data:
    00000000: 48 00 00 00 02 00 09 91 12 00 00 00 01 00 00 00
    00000010: 38 00 00 00 00 00 00 30 01 00 00 00 01 30 00 00
    00000020: 01 00 00 00 10 00 00 00 00 00 00 00 00 00 00 20
    00000030: 00 00 00 30 44 43 49 4d 00 00 00 00 00 00 00 00	DCIM
    00000040: 00 00 00 00 cc c3 01 46
 * 2 respargs: 0x0, 0x3c
 *
 * sample2:
 *
    00000000: 18 00 00 00 01 00 09 91 15 00 00 00 01 00 00 00
    00000010: 00 00 00 30 00 00 10 00
    00000000: 48 00 00 00 02 00 09 91 15 00 00 00 01 00 00 00
    00000010: 38 00 00 00 00 00 9c 33 01 00 00 00 01 30 00 00
    00000020: 01 00 00 00 10 00 00 00 00 00 00 00 00 00 00 30
    00000030: 00 00 9c 33 32 33 31 43 41 4e 4f 4e 00 00 00 00	 231CANON
    00000040: 00 00 00 00 cc c3 01 46
 */
#define PTP_OC_CANON_EOS_GetObjectInfoEx	0x9109
#define PTP_OC_CANON_EOS_GetThumbEx		0x910A
#define PTP_OC_CANON_EOS_SendPartialObject	0x910B
#define PTP_OC_CANON_EOS_SetObjectAttributes	0x910C
#define PTP_OC_CANON_EOS_GetObjectTime		0x910D
#define PTP_OC_CANON_EOS_SetObjectTime		0x910E

/* 910f: no args, no data, 1 response arg (0). */
#define PTP_OC_CANON_EOS_RemoteRelease		0x910F
/* Marcus: looks more like "Set DeviceProperty" in the trace.
 *
 * no cmd args
 * data phase (0xc, 0xd11c, 0x1)
 * no resp args
 */
#define PTP_OC_CANON_EOS_SetDevicePropValueEx	0x9110

// ### Magic Lantern
#define PTP_OC_CANON_EOS_SendObjectEx 0x9111
#define PTP_OC_CANON_EOS_CreateObject 0x9112

// ### Gphoto
#define PTP_OC_CANON_EOS_GetRemoteMode		0x9113
/* 9114: 1 arg (0x1), no data, no resp data. */
#define PTP_OC_CANON_EOS_SetRemoteMode		0x9114
/* 9115: 1 arg (0x1), no data, no resp data. */
#define PTP_OC_CANON_EOS_SetEventMode		0x9115
/* 9116: no args, data phase, no resp data. */
#define PTP_OC_CANON_EOS_GetEvent		0x9116
#define PTP_OC_CANON_EOS_TransferComplete	0x9117
#define PTP_OC_CANON_EOS_CancelTransfer		0x9118
#define PTP_OC_CANON_EOS_ResetTransfer		0x9119

/* 911a: 3 args (0xfffffff7, 0x00001000, 0x00000001), no data, no resp data. */
/* 911a: 3 args (0x001dfc60, 0x00001000, 0x00000001), no data, no resp data. */
#define PTP_OC_CANON_EOS_PCHDDCapacity		0x911A

/* 911b: no cmd args, no data, no resp args */
#define PTP_OC_CANON_EOS_SetUILock		0x911B
/* 911c: no cmd args, no data, no resp args */
#define PTP_OC_CANON_EOS_ResetUILock		0x911C
#define PTP_OC_CANON_EOS_KeepDeviceOn		0x911D /* no arg */
#define PTP_OC_CANON_EOS_SetNullPacketMode	0x911E /* 1 param */
#define PTP_OC_CANON_EOS_UpdateFirmware		0x911F
#define PTP_OC_CANON_EOS_TransferCompleteDT	0x9120
#define PTP_OC_CANON_EOS_CancelTransferDT	0x9121
#define PTP_OC_CANON_EOS_SetWftProfile		0x9122
#define PTP_OC_CANON_EOS_GetWftProfile		0x9123 /* 2 args: setnum, configid */
#define PTP_OC_CANON_EOS_SetProfileToWft	0x9124
#define PTP_OC_CANON_EOS_BulbStart		0x9125
#define PTP_OC_CANON_EOS_BulbEnd		0x9126
#define PTP_OC_CANON_EOS_RequestDevicePropValue	0x9127

/* 0x9128 args (0x1/0x2, 0x0), no data, no resp args */
#define PTP_OC_CANON_EOS_RemoteReleaseOn	0x9128
/* 0x9129 args (0x1/0x2), no data, no resp args */
#define PTP_OC_CANON_EOS_RemoteReleaseOff	0x9129

#define PTP_OC_CANON_EOS_RegistBackgroundImage	0x912A
#define PTP_OC_CANON_EOS_ChangePhotoStudioMode	0x912B
#define PTP_OC_CANON_EOS_GetPartialObjectEx	0x912C

// ### Magic Lantern
#define PTP_OC_CANON_EOS_ReSizeImageData 0x912D
#define PTP_OC_CANON_EOS_GetReSizeData 0x912E
#define PTP_OC_CANON_EOS_ReleaseReSizeData 0x912F

// ### Gphoto
#define PTP_OC_CANON_EOS_ResetMirrorLockupState	0x9130 /* no args */
#define PTP_OC_CANON_EOS_PopupBuiltinFlash	0x9131
#define PTP_OC_CANON_EOS_EndGetPartialObjectEx	0x9132
#define PTP_OC_CANON_EOS_MovieSelectSWOn	0x9133 /* no args */
#define PTP_OC_CANON_EOS_MovieSelectSWOff	0x9134 /* no args */
#define PTP_OC_CANON_EOS_GetCTGInfo		0x9135
#define PTP_OC_CANON_EOS_GetLensAdjust		0x9136
#define PTP_OC_CANON_EOS_SetLensAdjust		0x9137
#define PTP_OC_CANON_EOS_ReadyToSendMusic	0x9138 // Magic Lantern calls this GetMusicInfo
/* 3 paramaeters, no data, OFC, size, unknown */
#define PTP_OC_CANON_EOS_CreateHandle		0x9139
#define PTP_OC_CANON_EOS_SendPartialObjectEx	0x913A
#define PTP_OC_CANON_EOS_EndSendPartialObjectEx	0x913B
#define PTP_OC_CANON_EOS_SetCTGInfo		0x913C
#define PTP_OC_CANON_EOS_SetRequestOLCInfoGroup	0x913D
#define PTP_OC_CANON_EOS_SetRequestRollingPitchingLevel	0x913E /* 1 arg: onoff? */
/* 3 args, 0x21201020, 0x110, 0x1000000 (potentially reverse order) */

/* EOS M6 Mark2:	opargs: 0x01000000, 0x000001020, 0 (supportkind, modelid ?),
			response args: 0x00000811, 0x00000001 */

#define PTP_OC_CANON_EOS_GetCameraSupport	0x913F
#define PTP_OC_CANON_EOS_SetRating		0x9140 /* 2 args, objectid, rating? */
#define PTP_OC_CANON_EOS_RequestInnerDevelopStart	0x9141 /* 2 args: 1 type, 1 object? */
#define PTP_OC_CANON_EOS_RequestInnerDevelopParamChange	0x9142
#define PTP_OC_CANON_EOS_RequestInnerDevelopEnd		0x9143
#define PTP_OC_CANON_EOS_GpsLoggingDataMode		0x9144 /* 1 arg */
#define PTP_OC_CANON_EOS_GetGpsLogCurrentHandle		0x9145
#define PTP_OC_CANON_EOS_SetImageRecoveryData		0x9146 /* sends data? */
#define PTP_OC_CANON_EOS_GetImageRecoveryList		0x9147
#define PTP_OC_CANON_EOS_FormatImageRecoveryData	0x9148
#define PTP_OC_CANON_EOS_GetPresetLensAdjustParam	0x9149 /* no arg */
#define PTP_OC_CANON_EOS_GetRawDispImage		0x914A /* ? 2 args ? */
#define PTP_OC_CANON_EOS_SaveImageRecoveryData		0x914B
#define PTP_OC_CANON_EOS_RequestBLE			0x914C /* 2? args? */
#define PTP_OC_CANON_EOS_DrivePowerZoom			0x914D /* 1 arg */

// ### Magic Lantern
#define PTP_OC_CANON_EOS_SendTimeSyncMessage 0x914E

// ### Gphoto
#define PTP_OC_CANON_EOS_GetIptcData		0x914F
#define PTP_OC_CANON_EOS_SetIptcData		0x9150 /* sends data? */
#define PTP_OC_CANON_EOS_InitiateViewfinder	0x9151	/* no arg */
#define PTP_OC_CANON_EOS_TerminateViewfinder	0x9152
/* EOS M2 wlan: 2 params, 0x00200000 0x01000000 */
#define PTP_OC_CANON_EOS_GetViewFinderData	0x9153
#define PTP_OC_CANON_EOS_DoAf			0x9154
#define PTP_OC_CANON_EOS_DriveLens		0x9155
#define PTP_OC_CANON_EOS_DepthOfFieldPreview	0x9156 /* 1 arg */
#define PTP_OC_CANON_EOS_ClickWB		0x9157 /* 2 args: x,y */
#define PTP_OC_CANON_EOS_Zoom			0x9158 /* 1 arg: zoom */
#define PTP_OC_CANON_EOS_ZoomPosition		0x9159 /* 2 args: x,y */
#define PTP_OC_CANON_EOS_SetLiveAfFrame		0x915A /* sends data? */
#define PTP_OC_CANON_EOS_TouchAfPosition	0x915B /* 3 args: type,x,y */
#define PTP_OC_CANON_EOS_SetLvPcFlavoreditMode	0x915C /* 1 arg */
#define PTP_OC_CANON_EOS_SetLvPcFlavoreditParam	0x915D /* 1 arg */
#define PTP_OC_CANON_EOS_RequestSensorCleaning	0x915E /* 1 arg? */

// ### miniPtp
#define PTP_OC_CANON_EOS_SetTvForHiFreqFlicker		0x915F

// ### Gphoto
#define PTP_OC_CANON_EOS_AfCancel		0x9160

// ### miniPtp
#define PTP_OC_CANON_EOS_RequestAutoDetectOfHiFreqFlicker		0x9161
#define PTP_OC_CANON_EOS_CatchSubject		0x9162
#define PTP_OC_CANON_EOS_RequestAutoDetectOfFlicker		0x9163
#define PTP_OC_CANON_EOS_SetB2BFuncType		0x9164
#define PTP_OC_CANON_EOS_GetB2BFuncType		0x9165
#define PTP_OC_CANON_EOS_SetManipulationToggleImageSide		0x9166
#define PTP_OC_CANON_EOS_GetRegisteredLenses		0x9167

// ### Gphoto
#define PTP_OC_CANON_EOS_SetImageRecoveryDataEx	0x916B
#define PTP_OC_CANON_EOS_GetImageRecoveryListEx	0x916C
#define PTP_OC_CANON_EOS_CompleteAutoSendImages	0x916D
#define PTP_OC_CANON_EOS_NotifyAutoTransferStatus	0x916E
#define PTP_OC_CANON_EOS_GetReducedObject	0x916F
#define PTP_OC_CANON_EOS_GetObjectInfo64	0x9170	/* 1 arg: oid */
#define PTP_OC_CANON_EOS_GetObject64		0x9171	/* 1 arg: oid */
#define PTP_OC_CANON_EOS_GetPartialObject64	0x9172	/* args: oid, offset, maxbyte */
#define PTP_OC_CANON_EOS_GetObjectInfoEx64	0x9173	/* 2 args: storageid, oid  ? */
#define PTP_OC_CANON_EOS_GetPartialObjectEX64	0x9174	/* args: oid, offset 64bit, maxbyte */
#define PTP_OC_CANON_EOS_CreateHandle64		0x9175
#define PTP_OC_CANON_EOS_NotifySaveComplete	0x9177
#define PTP_OC_CANON_EOS_GetTranscodedBlock	0x9178
#define PTP_OC_CANON_EOS_TransferCompleteTranscodedBlock	0x9179
#define PTP_OC_CANON_EOS_NotifyEstimateNumberofImport		0x9182 /* 1 arg: importnumber */
#define PTP_OC_CANON_EOS_NotifyNumberofImported	0x9183 /* 1 arg: importnumber */
#define PTP_OC_CANON_EOS_NotifySizeOfPartialDataTransfer	0x9184 /* 4 args: filesizelow, filesizehigh, downloadsizelow, downloadsizehigh */

#define PTP_OC_CANON_EOS_NotifyFinish		0x9185 /* 1 arg: reason */
#define PTP_OC_CANON_EOS_GetWFTData		0x9186
#define PTP_OC_CANON_EOS_SetWFTData		0x9187
#define PTP_OC_CANON_EOS_ChangeWFTSettingNumber	0x9188
#define PTP_OC_CANON_EOS_GetPictureStylePCFlavorParam	0x9189
#define PTP_OC_CANON_EOS_SetPictureStylePCFlavorParam	0x918A
#define PTP_OC_CANON_EOS_GetObjectURL		0x91AB // magic lantern differed by case PTP_OC_CANON_EOS_GetObjectUrl
#define PTP_OC_CANON_EOS_SetCAssistMode		0x91AC
#define PTP_OC_CANON_EOS_GetCAssistPresetThumb	0x91AD

// ### miniPtp
#define PTP_OC_CANON_EOS_SendWifiSetting		0x91B1

// ### Gphoto
#define PTP_OC_CANON_EOS_SetFELock		0x91B9
#define PTP_OC_CANON_EOS_DeleteWFTSettingNumber	0x91BA
#define PTP_OC_CANON_EOS_SetDefaultCameraSetting		0x91BE
#define PTP_OC_CANON_EOS_GetAEData		0x91BF

// ### Magic Lantern
#define PTP_OC_CANON_EOS_ceresOpenFileValue 0x91C0
#define PTP_OC_CANON_EOS_ceresCreateFileValue 0x91C1
#define PTP_OC_CANON_EOS_ceresRemoveFileValue 0x91C2
#define PTP_OC_CANON_EOS_ceresCloseFileValue 0x91C3
#define PTP_OC_CANON_EOS_ceresGetWriteObject 0x91C4
#define PTP_OC_CANON_EOS_ceresSEndReadObject 0x91C5
#define PTP_OC_CANON_EOS_ceresFileAttributesValue 0x91C6
#define PTP_OC_CANON_EOS_ceresFileTimeValue 0x91C7
#define PTP_OC_CANON_EOS_ceresSeekFileValue 0x91C8
#define PTP_OC_CANON_EOS_ceresCreateDirectoryValue 0x91C9
#define PTP_OC_CANON_EOS_ceresRemoveDirectoryValue 0x91CA
#define PTP_OC_CANON_EOS_ceresSEndFileInfo 0x91CB
#define PTP_OC_CANON_EOS_ceresSEndFileInfoListEx 0x91CC
#define PTP_OC_CANON_EOS_ceresSEndDriveInfo 0x91CE
#define PTP_OC_CANON_EOS_ceresNotifyDriveStatus 0x91CF
#define PTP_OC_CANON_EOS_ceresSplitFileValue 0x91D0
#define PTP_OC_CANON_EOS_ceresRenameFileValue 0x91D1
#define PTP_OC_CANON_EOS_ceresTruncateFileValue 0x91D2
#define PTP_OC_CANON_EOS_SendCertData 0x91D4
#define PTP_OC_CANON_EOS_DistinctionRTC 0x91D7
#define PTP_OC_CANON_EOS_NotifyGpsTimeSyncStatus 0x91D8
#define PTP_OC_CANON_EOS_GetAdapterFirmData 0x91DF
#define PTP_OC_CANON_EOS_ceresSEndScanningResult 0x91E3

// ### Gphoto
// ML calls this PTP_OC_CANON_EOS_ceresSEndHostInfo
#define PTP_OC_CANON_EOS_SendHostInfo		0x91E4 /* https://research.checkpoint.com/say-cheese-ransomware-ing-a-dslr-camera/ */

// ### Magic Lantern
#define PTP_OC_CANON_EOS_NotifyAdapterStatus 0x91E6
//
// ### Gphoto
// ML calls this PTP_OC_CANON_EOS_ceresNotifyNetworkError
#define PTP_OC_CANON_EOS_NotifyNetworkError	0x91E8 /* 1 arg: errorcode */
#define PTP_OC_CANON_EOS_AdapterTransferProgress		0x91E9

// ### Magic Lantern
#define PTP_OC_CANON_EOS_ceresRequestAdapterProperty 0x91EA
#define PTP_OC_CANON_EOS_ceresSEndWpsPinCode 0x91EC
#define PTP_OC_CANON_EOS_ceresSEndWizardInfo 0x91ED

// ### miniPtp
#define PTP_OC_CANON_EOS_ceresSendBtSearchResult 0x91EF

// ### Gphoto
#define PTP_OC_CANON_EOS_TransferCompleteFTP	0x91F0
#define PTP_OC_CANON_EOS_CancelTransferFTP	0x91F1

// ### Magic Lantern
#define PTP_OC_CANON_EOS_ceresGetUpdateFileData 0x91F2
#define PTP_OC_CANON_EOS_NotifyUpdateProgress 0x91F3
#define PTP_OC_CANON_EOS_ceresSEndFactoryProperty 0x91F5
#define PTP_OC_CANON_EOS_ceresSEndGpsInfo 0x91F7
#define PTP_OC_CANON_EOS_ceresSEndBtPairingResult 0x91F8

// ### Gphoto
// ML calls this PTP_OC_CANON_EOS_ceresNotifyBtStatus 0x91F9
#define PTP_OC_CANON_EOS_NotifyBtStatus		0x91F9 /* https://research.checkpoint.com/say-cheese-ransomware-ing-a-dslr-camera/ */

// ### Magic Lantern
#define PTP_OC_CANON_EOS_SendTimeSyncInfo 0x91FC

// ### Gphoto
#define PTP_OC_CANON_EOS_SetAdapterBatteryReport		0x91FD /* https://research.checkpoint.com/say-cheese-ransomware-ing-a-dslr-camera/ */
#define PTP_OC_CANON_EOS_FAPIMessageTX		0x91FE
#define PTP_OC_CANON_EOS_FAPIMessageRX		0x91FF

// ### miniPtp
#define PTP_OC_CANON_RegisterEdgeFocusPosition	0x9200
#define PTP_OC_CANON_DriveFocusToEdge			0x9201
#define PTP_OC_CANON_DriveFocus					0x9203
#define PTP_OC_CANON_GetCurrentFocusPosition	0x9203
#define PTP_OC_CANON_SetPrivateKeyData			0x9205
#define PTP_OC_CANON_DeletePrivateKeyData		0x9206

#define PTP_OC_CANON_DV_GetWavInfo				0x9401
#define PTP_OC_CANON_DV_OverWriteContentsXml	0x9402
#define PTP_OC_CANON_DV_GetTemplateXml			0x9403 // miniPtp used Get_Template_Xml
#define PTP_OC_CANON_DV_SetTemplateXml			0x9404
#define PTP_OC_CANON_DV_GetNewsMetadataVersion	0x9405

/* Some Canon cams also support some MTP opcodes, usually 1-5 */
/* Microsoft / MTP extension codes */

#define PTP_OC_MTP_GetObjectPropsSupported	0x9801
#define PTP_OC_MTP_GetObjectPropDesc		0x9802
#define PTP_OC_MTP_GetObjectPropValue		0x9803
#define PTP_OC_MTP_SetObjectPropValue		0x9804
#define PTP_OC_MTP_GetObjPropList		0x9805
#define PTP_OC_MTP_SetObjPropList		0x9806
#define PTP_OC_MTP_GetInterdependendPropdesc	0x9807
#define PTP_OC_MTP_SendObjectPropList		0x9808
#define PTP_OC_MTP_GetObjectReferences		0x9810
#define PTP_OC_MTP_SetObjectReferences		0x9811
#define PTP_OC_MTP_UpdateDeviceFirmware		0x9812
#define PTP_OC_MTP_Skip				0x9820

typedef struct {
    const char *name;
    uint32_t op;
} ptp_op_name_t;

#define PTP_OC(name) {#name,name}

ptp_op_name_t ptp_op_names[]={
    PTP_OC(PTP_OC_GetDeviceInfo),
    PTP_OC(PTP_OC_OpenSession),
    PTP_OC(PTP_OC_CloseSession),
    PTP_OC(PTP_OC_GetStorageIDs),
    PTP_OC(PTP_OC_GetStorageInfo),
    PTP_OC(PTP_OC_GetNumObjects),
    PTP_OC(PTP_OC_GetObjectHandles),
    PTP_OC(PTP_OC_GetObjectInfo),
    PTP_OC(PTP_OC_GetObject),
    PTP_OC(PTP_OC_GetThumb),
    PTP_OC(PTP_OC_DeleteObject),
    PTP_OC(PTP_OC_SendObjectInfo),
    PTP_OC(PTP_OC_SendObject),
    PTP_OC(PTP_OC_InitiateCapture),
    PTP_OC(PTP_OC_FormatStore),
    PTP_OC(PTP_OC_ResetDevice),
    PTP_OC(PTP_OC_SelfTest),
    PTP_OC(PTP_OC_SetObjectProtection),
    PTP_OC(PTP_OC_PowerDown),
    PTP_OC(PTP_OC_GetDevicePropDesc),
    PTP_OC(PTP_OC_GetDevicePropValue),
    PTP_OC(PTP_OC_SetDevicePropValue),
    PTP_OC(PTP_OC_ResetDevicePropValue),
    PTP_OC(PTP_OC_TerminateOpenCapture),
    PTP_OC(PTP_OC_MoveObject),
    PTP_OC(PTP_OC_CopyObject),
    PTP_OC(PTP_OC_GetPartialObject),
    PTP_OC(PTP_OC_InitiateOpenCapture),
    PTP_OC(PTP_OC_StartEnumHandles),
    PTP_OC(PTP_OC_EnumHandles),
    PTP_OC(PTP_OC_StopEnumHandles),
    PTP_OC(PTP_OC_GetVendorExtensionMaps),
    PTP_OC(PTP_OC_GetVendorDeviceInfo),
    PTP_OC(PTP_OC_GetResizedImageObject),
    PTP_OC(PTP_OC_GetFilesystemManifest),
    PTP_OC(PTP_OC_GetStreamInfo),
    PTP_OC(PTP_OC_GetStream),

    PTP_OC(PTP_OC_CANON_GetPartialObjectInfo),
    PTP_OC(PTP_OC_CANON_SetObjectArchive),
    PTP_OC(PTP_OC_CANON_KeepDeviceOn),
    PTP_OC(PTP_OC_CANON_LockDeviceUI),
    PTP_OC(PTP_OC_CANON_UnlockDeviceUI),
    PTP_OC(PTP_OC_CANON_GetObjectHandleByName),
    PTP_OC(PTP_OC_CANON_InitiateReleaseControl),
    PTP_OC(PTP_OC_CANON_TerminateReleaseControl),
    PTP_OC(PTP_OC_CANON_TerminatePlaybackMode),
    PTP_OC(PTP_OC_CANON_ViewfinderOn),
    PTP_OC(PTP_OC_CANON_ViewfinderOff),
    PTP_OC(PTP_OC_CANON_DoAeAfAwb),

    PTP_OC(PTP_OC_CANON_GetCustomizeSpec),
    PTP_OC(PTP_OC_CANON_GetCustomizeItemInfo),
    PTP_OC(PTP_OC_CANON_GetCustomizeData),
    PTP_OC(PTP_OC_CANON_SetCustomizeData),
    PTP_OC(PTP_OC_CANON_GetCaptureStatus),
    PTP_OC(PTP_OC_CANON_CheckEvent),
    PTP_OC(PTP_OC_CANON_FocusLock),
    PTP_OC(PTP_OC_CANON_FocusUnlock),
    PTP_OC(PTP_OC_CANON_GetLocalReleaseParam),
    PTP_OC(PTP_OC_CANON_SetLocalReleaseParam),
    PTP_OC(PTP_OC_CANON_AskAboutPcEvf),
    PTP_OC(PTP_OC_CANON_SendPartialObject),
    PTP_OC(PTP_OC_CANON_InitiateCaptureInMemory),
    PTP_OC(PTP_OC_CANON_GetPartialObjectEx),
    PTP_OC(PTP_OC_CANON_SetObjectTime),
    PTP_OC(PTP_OC_CANON_GetViewfinderImage),
    PTP_OC(PTP_OC_CANON_GetObjectAttributes),
    PTP_OC(PTP_OC_CANON_ChangeUSBProtocol),
    PTP_OC(PTP_OC_CANON_GetChanges),
    PTP_OC(PTP_OC_CANON_GetObjectInfoEx),
    PTP_OC(PTP_OC_CANON_InitiateDirectTransfer),
    PTP_OC(PTP_OC_CANON_TerminateDirectTransfer),
    PTP_OC(PTP_OC_CANON_SendObjectInfoByPath),
    PTP_OC(PTP_OC_CANON_SendObjectByPath),
    PTP_OC(PTP_OC_CANON_InitiateDirectTansferEx),
    PTP_OC(PTP_OC_CANON_GetAncillaryObjectHandles),
    PTP_OC(PTP_OC_CANON_GetTreeInfo),
    PTP_OC(PTP_OC_CANON_GetTreeSize),
    PTP_OC(PTP_OC_CANON_NotifyProgress),
    PTP_OC(PTP_OC_CANON_NotifyCancelAccepted),
    PTP_OC(PTP_OC_CANON_GetDirectory),
    PTP_OC(PTP_OC_CANON_PingRequest),

    PTP_OC(PTP_OC_CANON_SetPairingInfo),
    PTP_OC(PTP_OC_CANON_GetPairingInfo),
    PTP_OC(PTP_OC_CANON_DeletePairingInfo),
    PTP_OC(PTP_OC_CANON_GetMACAddress),

    PTP_OC(PTP_OC_CANON_SetDisplayMonitor),
    PTP_OC(PTP_OC_CANON_PairingComplete),
    PTP_OC(PTP_OC_CANON_GetWirelessMAXChannel),

    PTP_OC(PTP_OC_CANON_InitiateEventProc0),
    PTP_OC(PTP_OC_CANON_TerminateEventProc_051),
    PTP_OC(PTP_OC_CANON_ExecuteEventProc),
    PTP_OC(PTP_OC_CANON_GetEventProcReturnData),
    PTP_OC(PTP_OC_CANON_IsEventProcRunning),
    PTP_OC(PTP_OC_CANON_QuerySizeOfTransparentMemory),
    PTP_OC(PTP_OC_CANON_LoadTransparentMemory),
    PTP_OC(PTP_OC_CANON_SaveTransparentMemory),
    PTP_OC(PTP_OC_CANON_QuickLoadTransparentMemory),
    PTP_OC(PTP_OC_CANON_InitiateEventProc1),
    PTP_OC(PTP_OC_CANON_TerminateEventProc_05D),

    PTP_OC(PTP_OC_CANON_GetWebServiceSpec),
    PTP_OC(PTP_OC_CANON_IsNeoKabotanProcMode),
    PTP_OC(PTP_OC_CANON_GetWebServiceData),
    PTP_OC(PTP_OC_CANON_SetWebServiceData),
    PTP_OC(PTP_OC_CANON_DeleteWebServiceData),
    PTP_OC(PTP_OC_CANON_GetRootCertificateSpec),
    PTP_OC(PTP_OC_CANON_GetRootCertificateData),
    PTP_OC(PTP_OC_CANON_SetRootCertificateData),
    PTP_OC(PTP_OC_CANON_DeleteRootCertificateData),
    PTP_OC(PTP_OC_CANON_GetGpsMobilelinkObjectInfo),
    PTP_OC(PTP_OC_CANON_SendGpsTagInfo),
    PTP_OC(PTP_OC_CANON_GetTranscodeApproxSize),
    PTP_OC(PTP_OC_CANON_RequestTranscodeStart),
    PTP_OC(PTP_OC_CANON_RequestTranscodeCancel),

    PTP_OC(PTP_OC_CANON_SetRemoteShootingMode),

    PTP_OC(PTP_OC_CANON_GetDebugLog),
    PTP_OC(PTP_OC_CANON_SetModeDialDisable),

    PTP_OC(PTP_OC_CANON_EOS_GetStorageIDs),
    PTP_OC(PTP_OC_CANON_EOS_GetStorageInfo),
    PTP_OC(PTP_OC_CANON_EOS_GetObjectInfo),
    PTP_OC(PTP_OC_CANON_EOS_GetObject),
    PTP_OC(PTP_OC_CANON_EOS_DeleteObject),
    PTP_OC(PTP_OC_CANON_EOS_FormatStore),
    PTP_OC(PTP_OC_CANON_EOS_GetPartialObject),
    PTP_OC(PTP_OC_CANON_EOS_GetDeviceInfoEx),

    PTP_OC(PTP_OC_CANON_EOS_GetObjectInfoEx),
    PTP_OC(PTP_OC_CANON_EOS_GetThumbEx),
    PTP_OC(PTP_OC_CANON_EOS_SendPartialObject),
    PTP_OC(PTP_OC_CANON_EOS_SetObjectAttributes),
    PTP_OC(PTP_OC_CANON_EOS_GetObjectTime),
    PTP_OC(PTP_OC_CANON_EOS_SetObjectTime),

    PTP_OC(PTP_OC_CANON_EOS_RemoteRelease),
    PTP_OC(PTP_OC_CANON_EOS_SetDevicePropValueEx),

    PTP_OC(PTP_OC_CANON_EOS_SendObjectEx),
    PTP_OC(PTP_OC_CANON_EOS_CreateObject),

    PTP_OC(PTP_OC_CANON_EOS_GetRemoteMode),
    PTP_OC(PTP_OC_CANON_EOS_SetRemoteMode),
    PTP_OC(PTP_OC_CANON_EOS_SetEventMode),
    PTP_OC(PTP_OC_CANON_EOS_GetEvent),
    PTP_OC(PTP_OC_CANON_EOS_TransferComplete),
    PTP_OC(PTP_OC_CANON_EOS_CancelTransfer),
    PTP_OC(PTP_OC_CANON_EOS_ResetTransfer),

    PTP_OC(PTP_OC_CANON_EOS_PCHDDCapacity),

    PTP_OC(PTP_OC_CANON_EOS_SetUILock),
    PTP_OC(PTP_OC_CANON_EOS_ResetUILock),
    PTP_OC(PTP_OC_CANON_EOS_KeepDeviceOn),
    PTP_OC(PTP_OC_CANON_EOS_SetNullPacketMode),
    PTP_OC(PTP_OC_CANON_EOS_UpdateFirmware),
    PTP_OC(PTP_OC_CANON_EOS_TransferCompleteDT),
    PTP_OC(PTP_OC_CANON_EOS_CancelTransferDT),
    PTP_OC(PTP_OC_CANON_EOS_SetWftProfile),
    PTP_OC(PTP_OC_CANON_EOS_GetWftProfile),
    PTP_OC(PTP_OC_CANON_EOS_SetProfileToWft),
    PTP_OC(PTP_OC_CANON_EOS_BulbStart),
    PTP_OC(PTP_OC_CANON_EOS_BulbEnd),
    PTP_OC(PTP_OC_CANON_EOS_RequestDevicePropValue),

    PTP_OC(PTP_OC_CANON_EOS_RemoteReleaseOn),
    PTP_OC(PTP_OC_CANON_EOS_RemoteReleaseOff),

    PTP_OC(PTP_OC_CANON_EOS_RegistBackgroundImage),
    PTP_OC(PTP_OC_CANON_EOS_ChangePhotoStudioMode),
    PTP_OC(PTP_OC_CANON_EOS_GetPartialObjectEx),

    PTP_OC(PTP_OC_CANON_EOS_ReSizeImageData),
    PTP_OC(PTP_OC_CANON_EOS_GetReSizeData),
    PTP_OC(PTP_OC_CANON_EOS_ReleaseReSizeData),

    PTP_OC(PTP_OC_CANON_EOS_ResetMirrorLockupState),
    PTP_OC(PTP_OC_CANON_EOS_PopupBuiltinFlash),
    PTP_OC(PTP_OC_CANON_EOS_EndGetPartialObjectEx),
    PTP_OC(PTP_OC_CANON_EOS_MovieSelectSWOn),
    PTP_OC(PTP_OC_CANON_EOS_MovieSelectSWOff),
    PTP_OC(PTP_OC_CANON_EOS_GetCTGInfo),
    PTP_OC(PTP_OC_CANON_EOS_GetLensAdjust),
    PTP_OC(PTP_OC_CANON_EOS_SetLensAdjust),
    PTP_OC(PTP_OC_CANON_EOS_ReadyToSendMusic),
    PTP_OC(PTP_OC_CANON_EOS_CreateHandle),
    PTP_OC(PTP_OC_CANON_EOS_SendPartialObjectEx),
    PTP_OC(PTP_OC_CANON_EOS_EndSendPartialObjectEx),
    PTP_OC(PTP_OC_CANON_EOS_SetCTGInfo),
    PTP_OC(PTP_OC_CANON_EOS_SetRequestOLCInfoGroup),
    PTP_OC(PTP_OC_CANON_EOS_SetRequestRollingPitchingLevel),


    PTP_OC(PTP_OC_CANON_EOS_GetCameraSupport),
    PTP_OC(PTP_OC_CANON_EOS_SetRating),
    PTP_OC(PTP_OC_CANON_EOS_RequestInnerDevelopStart),
    PTP_OC(PTP_OC_CANON_EOS_RequestInnerDevelopParamChange),
    PTP_OC(PTP_OC_CANON_EOS_RequestInnerDevelopEnd),
    PTP_OC(PTP_OC_CANON_EOS_GpsLoggingDataMode),
    PTP_OC(PTP_OC_CANON_EOS_GetGpsLogCurrentHandle),
    PTP_OC(PTP_OC_CANON_EOS_SetImageRecoveryData),
    PTP_OC(PTP_OC_CANON_EOS_GetImageRecoveryList),
    PTP_OC(PTP_OC_CANON_EOS_FormatImageRecoveryData),
    PTP_OC(PTP_OC_CANON_EOS_GetPresetLensAdjustParam),
    PTP_OC(PTP_OC_CANON_EOS_GetRawDispImage),
    PTP_OC(PTP_OC_CANON_EOS_SaveImageRecoveryData),
    PTP_OC(PTP_OC_CANON_EOS_RequestBLE),
    PTP_OC(PTP_OC_CANON_EOS_DrivePowerZoom),

    PTP_OC(PTP_OC_CANON_EOS_SendTimeSyncMessage),

    PTP_OC(PTP_OC_CANON_EOS_GetIptcData),
    PTP_OC(PTP_OC_CANON_EOS_SetIptcData),
    PTP_OC(PTP_OC_CANON_EOS_InitiateViewfinder),
    PTP_OC(PTP_OC_CANON_EOS_TerminateViewfinder),
    PTP_OC(PTP_OC_CANON_EOS_GetViewFinderData),
    PTP_OC(PTP_OC_CANON_EOS_DoAf),
    PTP_OC(PTP_OC_CANON_EOS_DriveLens),
    PTP_OC(PTP_OC_CANON_EOS_DepthOfFieldPreview),
    PTP_OC(PTP_OC_CANON_EOS_ClickWB),
    PTP_OC(PTP_OC_CANON_EOS_Zoom),
    PTP_OC(PTP_OC_CANON_EOS_ZoomPosition),
    PTP_OC(PTP_OC_CANON_EOS_SetLiveAfFrame),
    PTP_OC(PTP_OC_CANON_EOS_TouchAfPosition),
    PTP_OC(PTP_OC_CANON_EOS_SetLvPcFlavoreditMode),
    PTP_OC(PTP_OC_CANON_EOS_SetLvPcFlavoreditParam),
    PTP_OC(PTP_OC_CANON_EOS_RequestSensorCleaning),

    PTP_OC(PTP_OC_CANON_EOS_SetTvForHiFreqFlicker),

    PTP_OC(PTP_OC_CANON_EOS_AfCancel),

    PTP_OC(PTP_OC_CANON_EOS_RequestAutoDetectOfHiFreqFlicker),
    PTP_OC(PTP_OC_CANON_EOS_CatchSubject),
    PTP_OC(PTP_OC_CANON_EOS_RequestAutoDetectOfFlicker),
    PTP_OC(PTP_OC_CANON_EOS_SetB2BFuncType),
    PTP_OC(PTP_OC_CANON_EOS_GetB2BFuncType),
    PTP_OC(PTP_OC_CANON_EOS_SetManipulationToggleImageSide),
    PTP_OC(PTP_OC_CANON_EOS_GetRegisteredLenses),

    PTP_OC(PTP_OC_CANON_EOS_SetImageRecoveryDataEx),
    PTP_OC(PTP_OC_CANON_EOS_GetImageRecoveryListEx),
    PTP_OC(PTP_OC_CANON_EOS_CompleteAutoSendImages),
    PTP_OC(PTP_OC_CANON_EOS_NotifyAutoTransferStatus),
    PTP_OC(PTP_OC_CANON_EOS_GetReducedObject),
    PTP_OC(PTP_OC_CANON_EOS_GetObjectInfo64),
    PTP_OC(PTP_OC_CANON_EOS_GetObject64),
    PTP_OC(PTP_OC_CANON_EOS_GetPartialObject64),
    PTP_OC(PTP_OC_CANON_EOS_GetObjectInfoEx64),
    PTP_OC(PTP_OC_CANON_EOS_GetPartialObjectEX64),
    PTP_OC(PTP_OC_CANON_EOS_CreateHandle64),
    PTP_OC(PTP_OC_CANON_EOS_NotifySaveComplete),
    PTP_OC(PTP_OC_CANON_EOS_GetTranscodedBlock),
    PTP_OC(PTP_OC_CANON_EOS_TransferCompleteTranscodedBlock),
    PTP_OC(PTP_OC_CANON_EOS_NotifyEstimateNumberofImport),
    PTP_OC(PTP_OC_CANON_EOS_NotifyNumberofImported),
    PTP_OC(PTP_OC_CANON_EOS_NotifySizeOfPartialDataTransfer),

    PTP_OC(PTP_OC_CANON_EOS_NotifyFinish),
    PTP_OC(PTP_OC_CANON_EOS_GetWFTData),
    PTP_OC(PTP_OC_CANON_EOS_SetWFTData),
    PTP_OC(PTP_OC_CANON_EOS_ChangeWFTSettingNumber),
    PTP_OC(PTP_OC_CANON_EOS_GetPictureStylePCFlavorParam),
    PTP_OC(PTP_OC_CANON_EOS_SetPictureStylePCFlavorParam),
    PTP_OC(PTP_OC_CANON_EOS_GetObjectURL),
    PTP_OC(PTP_OC_CANON_EOS_SetCAssistMode),
    PTP_OC(PTP_OC_CANON_EOS_GetCAssistPresetThumb),

    PTP_OC(PTP_OC_CANON_EOS_SendWifiSetting),

    PTP_OC(PTP_OC_CANON_EOS_SetFELock),
    PTP_OC(PTP_OC_CANON_EOS_DeleteWFTSettingNumber),
    PTP_OC(PTP_OC_CANON_EOS_SetDefaultCameraSetting),
    PTP_OC(PTP_OC_CANON_EOS_GetAEData),

    PTP_OC(PTP_OC_CANON_EOS_ceresOpenFileValue),
    PTP_OC(PTP_OC_CANON_EOS_ceresCreateFileValue),
    PTP_OC(PTP_OC_CANON_EOS_ceresRemoveFileValue),
    PTP_OC(PTP_OC_CANON_EOS_ceresCloseFileValue),
    PTP_OC(PTP_OC_CANON_EOS_ceresGetWriteObject),
    PTP_OC(PTP_OC_CANON_EOS_ceresSEndReadObject),
    PTP_OC(PTP_OC_CANON_EOS_ceresFileAttributesValue),
    PTP_OC(PTP_OC_CANON_EOS_ceresFileTimeValue),
    PTP_OC(PTP_OC_CANON_EOS_ceresSeekFileValue),
    PTP_OC(PTP_OC_CANON_EOS_ceresCreateDirectoryValue),
    PTP_OC(PTP_OC_CANON_EOS_ceresRemoveDirectoryValue),
    PTP_OC(PTP_OC_CANON_EOS_ceresSEndFileInfo),
    PTP_OC(PTP_OC_CANON_EOS_ceresSEndFileInfoListEx),
    PTP_OC(PTP_OC_CANON_EOS_ceresSEndDriveInfo),
    PTP_OC(PTP_OC_CANON_EOS_ceresNotifyDriveStatus),
    PTP_OC(PTP_OC_CANON_EOS_ceresSplitFileValue),
    PTP_OC(PTP_OC_CANON_EOS_ceresRenameFileValue),
    PTP_OC(PTP_OC_CANON_EOS_ceresTruncateFileValue),
    PTP_OC(PTP_OC_CANON_EOS_SendCertData),
    PTP_OC(PTP_OC_CANON_EOS_DistinctionRTC),
    PTP_OC(PTP_OC_CANON_EOS_NotifyGpsTimeSyncStatus),
    PTP_OC(PTP_OC_CANON_EOS_GetAdapterFirmData),
    PTP_OC(PTP_OC_CANON_EOS_ceresSEndScanningResult),

    PTP_OC(PTP_OC_CANON_EOS_SendHostInfo),

    PTP_OC(PTP_OC_CANON_EOS_NotifyAdapterStatus),

    PTP_OC(PTP_OC_CANON_EOS_NotifyNetworkError),
    PTP_OC(PTP_OC_CANON_EOS_AdapterTransferProgress),

    PTP_OC(PTP_OC_CANON_EOS_ceresRequestAdapterProperty),
    PTP_OC(PTP_OC_CANON_EOS_ceresSEndWpsPinCode),
    PTP_OC(PTP_OC_CANON_EOS_ceresSEndWizardInfo),

    PTP_OC(PTP_OC_CANON_EOS_ceresSendBtSearchResult),

    PTP_OC(PTP_OC_CANON_EOS_TransferCompleteFTP),
    PTP_OC(PTP_OC_CANON_EOS_CancelTransferFTP),

    PTP_OC(PTP_OC_CANON_EOS_ceresGetUpdateFileData),
    PTP_OC(PTP_OC_CANON_EOS_NotifyUpdateProgress),
    PTP_OC(PTP_OC_CANON_EOS_ceresSEndFactoryProperty),
    PTP_OC(PTP_OC_CANON_EOS_ceresSEndGpsInfo),
    PTP_OC(PTP_OC_CANON_EOS_ceresSEndBtPairingResult),

    PTP_OC(PTP_OC_CANON_EOS_NotifyBtStatus),

    PTP_OC(PTP_OC_CANON_EOS_SendTimeSyncInfo),

    PTP_OC(PTP_OC_CANON_EOS_SetAdapterBatteryReport),
    PTP_OC(PTP_OC_CANON_EOS_FAPIMessageTX),
    PTP_OC(PTP_OC_CANON_EOS_FAPIMessageRX),

    PTP_OC(PTP_OC_CANON_RegisterEdgeFocusPosition),
    PTP_OC(PTP_OC_CANON_DriveFocusToEdge),
    PTP_OC(PTP_OC_CANON_DriveFocus),
    PTP_OC(PTP_OC_CANON_GetCurrentFocusPosition),
    PTP_OC(PTP_OC_CANON_SetPrivateKeyData),
    PTP_OC(PTP_OC_CANON_DeletePrivateKeyData),

    PTP_OC(PTP_OC_CANON_DV_GetWavInfo),
    PTP_OC(PTP_OC_CANON_DV_OverWriteContentsXml),
    PTP_OC(PTP_OC_CANON_DV_GetTemplateXml),
    PTP_OC(PTP_OC_CANON_DV_SetTemplateXml),
    PTP_OC(PTP_OC_CANON_DV_GetNewsMetadataVersion),

    PTP_OC(PTP_OC_MTP_GetObjectPropsSupported),
    PTP_OC(PTP_OC_MTP_GetObjectPropDesc),
    PTP_OC(PTP_OC_MTP_GetObjectPropValue),
    PTP_OC(PTP_OC_MTP_SetObjectPropValue),
    PTP_OC(PTP_OC_MTP_GetObjPropList),
    PTP_OC(PTP_OC_MTP_SetObjPropList),
    PTP_OC(PTP_OC_MTP_GetInterdependendPropdesc),
    PTP_OC(PTP_OC_MTP_SendObjectPropList),
    PTP_OC(PTP_OC_MTP_GetObjectReferences),
    PTP_OC(PTP_OC_MTP_SetObjectReferences),
    PTP_OC(PTP_OC_MTP_UpdateDeviceFirmware),
    PTP_OC(PTP_OC_MTP_Skip),
};

const char *get_ptp_op_name(uint32_t op) {
    unsigned i;
    for(i=0;i<(sizeof(ptp_op_names)/sizeof(ptp_op_name_t)); i++) {
        if(op==ptp_op_names[i].op) {
            return ptp_op_names[i].name;
        }
    }
    return NULL;
}

