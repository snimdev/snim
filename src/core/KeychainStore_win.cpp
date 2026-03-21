#include "core/KeychainStore.h"

#include <windows.h>
#include <wincred.h>

#include <string>

// Windows backing for Core::KeychainStore: one generic credential per "service/account", UTF-8.
namespace Core::KeychainStore {

namespace {

std::wstring targetFor(const QString &service, const QString &account)
{
    return (service + QLatin1Char('/') + account).toStdWString();
}

} // namespace

bool store(const QString &service, const QString &account, const QString &secret)
{
    if (service.isEmpty() || account.isEmpty())
        return false;
    std::wstring target = targetFor(service, account);
    std::wstring user = account.toStdWString();
    QByteArray blob = secret.toUtf8();

    // CredWriteW replaces an existing entry with the same target, so this is an upsert.
    CREDENTIALW cred{};
    cred.Type = CRED_TYPE_GENERIC;
    cred.TargetName = target.data();
    cred.UserName = user.data();
    cred.CredentialBlobSize = static_cast<DWORD>(blob.size());
    cred.CredentialBlob = reinterpret_cast<LPBYTE>(blob.data());
    cred.Persist = CRED_PERSIST_LOCAL_MACHINE;   // this user on this machine, never roams
    const bool ok = CredWriteW(&cred, 0) != FALSE;
    SecureZeroMemory(blob.data(), static_cast<SIZE_T>(blob.size()));
    return ok;
}

std::optional<QString> retrieve(const QString &service, const QString &account)
{
    if (service.isEmpty() || account.isEmpty())
        return std::nullopt;
    const std::wstring target = targetFor(service, account);
    PCREDENTIALW cred = nullptr;
    if (!CredReadW(target.c_str(), CRED_TYPE_GENERIC, 0, &cred) || !cred)
        return std::nullopt;
    const QString value = QString::fromUtf8(reinterpret_cast<const char *>(cred->CredentialBlob),
                                            static_cast<qsizetype>(cred->CredentialBlobSize));
    CredFree(cred);
    return value;
}

bool erase(const QString &service, const QString &account)
{
    if (service.isEmpty() || account.isEmpty())
        return false;
    const std::wstring target = targetFor(service, account);
    return CredDeleteW(target.c_str(), CRED_TYPE_GENERIC, 0) != FALSE
           || GetLastError() == ERROR_NOT_FOUND;
}

} // namespace Core::KeychainStore
