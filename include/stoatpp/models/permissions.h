#pragma once
#include <cstdint>

namespace stoatpp::permissions {

// Revolt permission flags
constexpr int64_t ManageChannels      = 1ULL << 0;
constexpr int64_t ManageServer        = 1ULL << 1;
constexpr int64_t ManagePermissions   = 1ULL << 2;
constexpr int64_t ManageRole          = 1ULL << 3;
constexpr int64_t ManageCustomisation = 1ULL << 4;

// Member permissions
constexpr int64_t KickMembers         = 1ULL << 6;
constexpr int64_t BanMembers          = 1ULL << 7;
constexpr int64_t TimeoutMembers      = 1ULL << 8;
constexpr int64_t AssignRoles         = 1ULL << 9;
constexpr int64_t ChangeNickname      = 1ULL << 10;
constexpr int64_t ManageNicknames     = 1ULL << 11;
constexpr int64_t ManageAvatar        = 1ULL << 12;
constexpr int64_t RemoveAvatars       = 1ULL << 13;

// Channel permissions
constexpr int64_t ViewChannel         = 1ULL << 20;
constexpr int64_t ReadMessageHistory  = 1ULL << 21;
constexpr int64_t SendMessages        = 1ULL << 22;
constexpr int64_t ManageMessages      = 1ULL << 23;
constexpr int64_t ManageWebhooks      = 1ULL << 24;
constexpr int64_t CreateInvites       = 1ULL << 25;
constexpr int64_t EmbedLinks          = 1ULL << 26;
constexpr int64_t UploadFiles         = 1ULL << 27;
constexpr int64_t Masquerade          = 1ULL << 28;
constexpr int64_t AddReactions        = 1ULL << 29;

} // namespace stoatpp::permissions
