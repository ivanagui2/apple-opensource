// This file was automatically generated by protocompiler
// DO NOT EDIT!
// Compiled from stdin

#import <Foundation/Foundation.h>
#import <ProtocolBuffer/PBCodable.h>

#import "AWDIPMonitorGlobalEnums.h"

typedef NS_ENUM(int32_t, AWDIPMonitorInterfaceAdvisoryReport_Flags) {
    AWDIPMonitorInterfaceAdvisoryReport_Flags_LINK_LAYER_ISSUE = 1,
    AWDIPMonitorInterfaceAdvisoryReport_Flags_UPLINK_ISSUE = 2,
};
#ifdef __OBJC__
NS_INLINE NSString *AWDIPMonitorInterfaceAdvisoryReport_FlagsAsString(AWDIPMonitorInterfaceAdvisoryReport_Flags value)
{
    switch (value)
    {
        case AWDIPMonitorInterfaceAdvisoryReport_Flags_LINK_LAYER_ISSUE: return @"LINK_LAYER_ISSUE";
        case AWDIPMonitorInterfaceAdvisoryReport_Flags_UPLINK_ISSUE: return @"UPLINK_ISSUE";
        default: return [NSString stringWithFormat:@"(unknown: %i)", value];
    }
}
#endif /* __OBJC__ */
#ifdef __OBJC__
NS_INLINE AWDIPMonitorInterfaceAdvisoryReport_Flags StringAsAWDIPMonitorInterfaceAdvisoryReport_Flags(NSString *value)
{
    if ([value isEqualToString:@"LINK_LAYER_ISSUE"]) return AWDIPMonitorInterfaceAdvisoryReport_Flags_LINK_LAYER_ISSUE;
    if ([value isEqualToString:@"UPLINK_ISSUE"]) return AWDIPMonitorInterfaceAdvisoryReport_Flags_UPLINK_ISSUE;
    return AWDIPMonitorInterfaceAdvisoryReport_Flags_LINK_LAYER_ISSUE;
}
#endif /* __OBJC__ */

#ifdef __cplusplus
#define AWDIPMONITORINTERFACEADVISORYREPORT_FUNCTION extern "C"
#else
#define AWDIPMONITORINTERFACEADVISORYREPORT_FUNCTION extern
#endif

@interface AWDIPMonitorInterfaceAdvisoryReport : PBCodable <NSCopying>
{
    uint64_t _timestamp;
    uint32_t _advisoryCount;
    uint32_t _flags;
    AWDIPMonitorInterfaceType _interfaceType;
    struct {
        int timestamp:1;
        int advisoryCount:1;
        int flags:1;
        int interfaceType:1;
    } _has;
}


@property (nonatomic) BOOL hasTimestamp;
@property (nonatomic) uint64_t timestamp;

@property (nonatomic) BOOL hasInterfaceType;
@property (nonatomic) AWDIPMonitorInterfaceType interfaceType;
- (NSString *)interfaceTypeAsString:(AWDIPMonitorInterfaceType)value;
- (AWDIPMonitorInterfaceType)StringAsInterfaceType:(NSString *)str;

@property (nonatomic) BOOL hasFlags;
@property (nonatomic) uint32_t flags;

@property (nonatomic) BOOL hasAdvisoryCount;
@property (nonatomic) uint32_t advisoryCount;

// Performs a shallow copy into other
- (void)copyTo:(AWDIPMonitorInterfaceAdvisoryReport *)other;

// Performs a deep merge from other into self
// If set in other, singular values in self are replaced in self
// Singular composite values are recursively merged
// Repeated values from other are appended to repeated values in self
- (void)mergeFrom:(AWDIPMonitorInterfaceAdvisoryReport *)other;

AWDIPMONITORINTERFACEADVISORYREPORT_FUNCTION BOOL AWDIPMonitorInterfaceAdvisoryReportReadFrom(AWDIPMonitorInterfaceAdvisoryReport *self, PBDataReader *reader);

@end
