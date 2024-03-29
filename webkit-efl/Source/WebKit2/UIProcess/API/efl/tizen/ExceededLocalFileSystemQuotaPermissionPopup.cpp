/*
   Copyright (C) 2014 Samsung Electronics

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#include "ExceededLocalFileSystemQuotaPermissionPopup.h"

#if ENABLE(TIZEN_FILE_SYSTEM)

#include "ewk_view.h"

namespace WebKit {

ExceededLocalFileSystemQuotaPermissionPopup::ExceededLocalFileSystemQuotaPermissionPopup(const Ewk_Security_Origin* origin, const String& message)
    : PermissionPopup(origin, message)
{
}

void ExceededLocalFileSystemQuotaPermissionPopup::sendDecidedPermission(Evas_Object* ewkView, bool isDecided)
{
    ewk_view_exceeded_local_file_system_quota_reply(ewkView, isDecided);
}

} // namespace WebKit

#endif // ENABLE(TIZEN_FILE_SYSTEM)
