/***************************************************************************
 *   Copyright (C) 2009 - 2010 by Simon Qian <SimonQian@SimonQian.com>     *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/
#ifndef __FAKEFAT32_FS_H_INCLUDED__
#define __FAKEFAT32_FS_H_INCLUDED__

#include "httpd_fs.h"

static const uint8_t vsfcdc_inf[] =
"\
[Version]\r\n\
Signature=\"$Windows NT$\"\r\n\
Class=Ports\r\n\
ClassGuid={4D36E978-E325-11CE-BFC1-08002BE10318}\r\n\
Provider=%PRVDR%\r\n\
CatalogFile=VSFCDC.cat\r\n\
DriverVer=04/25/2010,1.3.1\r\n\
\r\n\
[SourceDisksNames]\r\n\
1=%DriversDisk%,,,\r\n\
\r\n\
[SourceDisksFiles]\r\n\
\r\n\
[Manufacturer]\r\n\
%MFGNAME%=DeviceList,NT,NTamd64\r\n\
\r\n\
[DestinationDirs]\r\n\
DefaultDestDir = 12\r\n\
\r\n\
;------------------------------------------------------------------------------\r\n\
;            VID/PID Settings\r\n\
;------------------------------------------------------------------------------\r\n\
[DeviceList.NT]\r\n\
%DESCRIPTION%=DriverInstall,USB\\VID_" GENERATE_STR(APPCFG_USBD_VID) "&PID_" GENERATE_STR(APPCFG_USBD_PID) "&MI_00\r\n\
\r\n\
[DeviceList.NTamd64]\r\n\
%DESCRIPTION%=DriverInstall,USB\\VID_" GENERATE_STR(APPCFG_USBD_VID) "&PID_" GENERATE_STR(APPCFG_USBD_PID) "&MI_00\r\n\
\r\n\
[DriverInstall.NT]\r\n\
Include=mdmcpq.inf\r\n\
CopyFiles=FakeModemCopyFileSection\r\n\
AddReg=DriverInstall.NT.AddReg\r\n\
\r\n\
[DriverInstall.NT.AddReg]\r\n\
HKR,,DevLoader,,*ntkern\r\n\
HKR,,NTMPDriver,,usbser.sys\r\n\
HKR,,EnumPropPages32,,\"MsPorts.dll,SerialPortPropPageProvider\"\r\n\
\r\n\
[DriverInstall.NT.Services]\r\n\
AddService=usbser, 0x00000002, DriverServiceInst\r\n\
\r\n\
[DriverServiceInst]\r\n\
DisplayName=%SERVICE%\r\n\
ServiceType = 1 ; SERVICE_KERNEL_DRIVER\r\n\
StartType = 3 ; SERVICE_DEMAND_START\r\n\
ErrorControl = 1 ; SERVICE_ERROR_NORMAL\r\n\
ServiceBinary= %12%\\usbser.sys\r\n\
LoadOrderGroup = Base\r\n\
\r\n\
;------------------------------------------------------------------------------\r\n\
;              String Definitions\r\n\
;------------------------------------------------------------------------------\r\n\
\r\n\
[Strings]\r\n\
PRVDR = \"VSF\"\r\n\
MFGNAME = \"VSF.\"\r\n\
DESCRIPTION = \"VSFCDC\"\r\n\
SERVICE = \"VSFCDC\"\r\n\
DriversDisk = \"VSF Drivers Disk\" \
";

static struct fakefat32_file_t fakefat32_windows_dir[] =
{
	{
		.memfile.file.name = ".",
		.memfile.file.attr = VSFILE_ATTR_DIRECTORY,
	},
	{
		.memfile.file.name = "..",
		.memfile.file.attr = VSFILE_ATTR_DIRECTORY,
	},
	{
		.memfile.file.name = "VSFCDC.inf",
		.memfile.file.size = sizeof(vsfcdc_inf) - 1,
		.memfile.file.attr = VSFILE_ATTR_ARCHIVE | VSFILE_ATTR_READONLY,
		.memfile.f.buff = (uint8_t *)vsfcdc_inf,
	},
	{
		.memfile.file.name = NULL,
	},
};
static struct fakefat32_file_t fakefat32_driver_dir[] =
{
	{
		.memfile.file.name = ".",
		.memfile.file.attr = VSFILE_ATTR_DIRECTORY,
	},
	{
		.memfile.file.name = "..",
		.memfile.file.attr = VSFILE_ATTR_DIRECTORY,
	},
	{
		.memfile.file.name = "Windows",
		.memfile.file.attr = VSFILE_ATTR_DIRECTORY,
		.memfile.d.child = (struct vsfile_memfile_t *)fakefat32_windows_dir,
	},
	{
		.memfile.file.name = NULL,
	},
};
static struct fakefat32_file_t fakefat32_root_dir[] =
{
	{
		.memfile.file.name = "VSFAIO",
		.memfile.file.attr = VSFILE_ATTR_VOLUMID,
	},
	{
		.memfile.file.name = "Driver",
		.memfile.file.attr = VSFILE_ATTR_DIRECTORY,
		.memfile.d.child = (struct vsfile_memfile_t *)fakefat32_driver_dir,
	},
	{
		.memfile.file.name = "HttpRoot",
		.memfile.file.attr = VSFILE_ATTR_DIRECTORY,
		.memfile.d.child = (struct vsfile_memfile_t *)httpd_root_dir,
	},
	{
		.memfile.file.name = NULL,
	},
};

#endif		// __FAKEFAT32_FS_H_INCLUDED__
