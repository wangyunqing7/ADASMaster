#pragma once

// =============================================================================
// UserAccount.h
// 功能：登录账号的强类型表示（用户名、显示名、角色、密码哈希）。
//       供 AuthManager 从 users.json 加载后持有。
// =============================================================================

#include <QString>

struct UserAccount
{
    QString username;
    QString displayName;
    QString role;          // "visitor"/"operator"/"engineer"/"admin"
    QString passwordHash;  // SHA256 十六进制
};
