#include "core/KeychainStore.h"

#import <Foundation/Foundation.h>
#import <Security/Security.h>

// macOS keychain backing for Core::KeychainStore. Built with -fobjc-arc (ARC manages
// the NS* objects; the Security.framework CF results are released manually). Stores a
// generic password keyed by (service, account).
namespace Core::KeychainStore {

static NSDictionary *baseQuery(const QString &service, const QString &account)
{
    return @{
        (__bridge id)kSecClass        : (__bridge id)kSecClassGenericPassword,
        (__bridge id)kSecAttrService  : service.toNSString(),
        (__bridge id)kSecAttrAccount  : account.toNSString(),
    };
}

bool store(const QString &service, const QString &account, const QString &secret, Failure *why)
{
    if (why)
        *why = Failure::Other;
    if (service.isEmpty() || account.isEmpty())
        return false;
    // Upsert: delete any existing item, then add (avoids errSecDuplicateItem).
    SecItemDelete((__bridge CFDictionaryRef)baseQuery(service, account));

    NSMutableDictionary *add = [baseQuery(service, account) mutableCopy];
    add[(__bridge id)kSecValueData] = [secret.toNSString() dataUsingEncoding:NSUTF8StringEncoding];
    add[(__bridge id)kSecAttrAccessible] = (__bridge id)kSecAttrAccessibleWhenUnlocked;
    const OSStatus status = SecItemAdd((__bridge CFDictionaryRef)add, nullptr);
    if (status == errSecSuccess && why)
        *why = Failure::None;
    return status == errSecSuccess;
}

std::optional<QString> retrieve(const QString &service, const QString &account, Failure *why)
{
    if (why)
        *why = Failure::Other;
    if (service.isEmpty() || account.isEmpty())
        return std::nullopt;
    NSMutableDictionary *query = [baseQuery(service, account) mutableCopy];
    query[(__bridge id)kSecReturnData] = @YES;
    query[(__bridge id)kSecMatchLimit] = (__bridge id)kSecMatchLimitOne;

    CFTypeRef result = nullptr;
    const OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)query, &result);
    if (status == errSecItemNotFound && why)
        *why = Failure::None;
    if (status != errSecSuccess || !result)
        return std::nullopt;
    NSData *data = (__bridge_transfer NSData *)result;   // take ownership, ARC releases it
    NSString *value = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
    if (!value)
        return std::nullopt;
    if (why)
        *why = Failure::None;
    return QString::fromNSString(value);
}

bool erase(const QString &service, const QString &account)
{
    if (service.isEmpty() || account.isEmpty())
        return false;
    const OSStatus status = SecItemDelete((__bridge CFDictionaryRef)baseQuery(service, account));
    return status == errSecSuccess || status == errSecItemNotFound;
}

} // namespace Core::KeychainStore
