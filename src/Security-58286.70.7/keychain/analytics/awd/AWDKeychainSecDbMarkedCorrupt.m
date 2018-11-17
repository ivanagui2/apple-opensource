// This file was automatically generated by protocompiler
// DO NOT EDIT!
// Compiled from stdin

#import <TargetConditionals.h>
#if !TARGET_OS_BRIDGE

#import "AWDKeychainSecDbMarkedCorrupt.h"
#import <ProtocolBuffer/PBConstants.h>
#import <ProtocolBuffer/PBHashUtil.h>
#import <ProtocolBuffer/PBDataReader.h>

#if !__has_feature(objc_arc)
# error This generated file depends on ARC but it is not enabled; turn on ARC, or use 'objc_use_arc' option to generate non-ARC code.
#endif

@implementation AWDKeychainSecDbMarkedCorrupt

@synthesize timestamp = _timestamp;
- (void)setTimestamp:(uint64_t)v
{
    _has.timestamp = YES;
    _timestamp = v;
}
- (void)setHasTimestamp:(BOOL)f
{
    _has.timestamp = f;
}
- (BOOL)hasTimestamp
{
    return _has.timestamp;
}
@synthesize reason = _reason;
- (void)setReason:(uint32_t)v
{
    _has.reason = YES;
    _reason = v;
}
- (void)setHasReason:(BOOL)f
{
    _has.reason = f;
}
- (BOOL)hasReason
{
    return _has.reason;
}
@synthesize sqlitecode = _sqlitecode;
- (void)setSqlitecode:(uint32_t)v
{
    _has.sqlitecode = YES;
    _sqlitecode = v;
}
- (void)setHasSqlitecode:(BOOL)f
{
    _has.sqlitecode = f;
}
- (BOOL)hasSqlitecode
{
    return _has.sqlitecode;
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"%@ %@", [super description], [self dictionaryRepresentation]];
}

- (NSDictionary *)dictionaryRepresentation
{
    NSMutableDictionary *dict = [NSMutableDictionary dictionary];
    if (self->_has.timestamp)
    {
        [dict setObject:[NSNumber numberWithUnsignedLongLong:self->_timestamp] forKey:@"timestamp"];
    }
    if (self->_has.reason)
    {
        [dict setObject:[NSNumber numberWithUnsignedInt:self->_reason] forKey:@"reason"];
    }
    if (self->_has.sqlitecode)
    {
        [dict setObject:[NSNumber numberWithUnsignedInt:self->_sqlitecode] forKey:@"sqlitecode"];
    }
    return dict;
}

BOOL AWDKeychainSecDbMarkedCorruptReadFrom(__unsafe_unretained AWDKeychainSecDbMarkedCorrupt *self, __unsafe_unretained PBDataReader *reader) {
    while (PBReaderHasMoreData(reader)) {
        uint32_t tag = 0;
        uint8_t aType = 0;

        PBReaderReadTag32AndType(reader, &tag, &aType);

        if (PBReaderHasError(reader))
            break;

        if (aType == TYPE_END_GROUP) {
            break;
        }

        switch (tag) {

            case 1 /* timestamp */:
            {
                self->_has.timestamp = YES;
                self->_timestamp = PBReaderReadUint64(reader);
            }
            break;
            case 2 /* reason */:
            {
                self->_has.reason = YES;
                self->_reason = PBReaderReadUint32(reader);
            }
            break;
            case 3 /* sqlitecode */:
            {
                self->_has.sqlitecode = YES;
                self->_sqlitecode = PBReaderReadUint32(reader);
            }
            break;
            default:
                if (!PBReaderSkipValueWithTag(reader, tag, aType))
                    return NO;
                break;
        }
    }
    return !PBReaderHasError(reader);
}

- (BOOL)readFrom:(PBDataReader *)reader
{
    return AWDKeychainSecDbMarkedCorruptReadFrom(self, reader);
}
- (void)writeTo:(PBDataWriter *)writer
{
    /* timestamp */
    {
        if (self->_has.timestamp)
        {
            PBDataWriterWriteUint64Field(writer, self->_timestamp, 1);
        }
    }
    /* reason */
    {
        if (self->_has.reason)
        {
            PBDataWriterWriteUint32Field(writer, self->_reason, 2);
        }
    }
    /* sqlitecode */
    {
        if (self->_has.sqlitecode)
        {
            PBDataWriterWriteUint32Field(writer, self->_sqlitecode, 3);
        }
    }
}

- (void)copyTo:(AWDKeychainSecDbMarkedCorrupt *)other
{
    if (self->_has.timestamp)
    {
        other->_timestamp = _timestamp;
        other->_has.timestamp = YES;
    }
    if (self->_has.reason)
    {
        other->_reason = _reason;
        other->_has.reason = YES;
    }
    if (self->_has.sqlitecode)
    {
        other->_sqlitecode = _sqlitecode;
        other->_has.sqlitecode = YES;
    }
}

- (id)copyWithZone:(NSZone *)zone
{
    AWDKeychainSecDbMarkedCorrupt *copy = [[[self class] allocWithZone:zone] init];
    if (self->_has.timestamp)
    {
        copy->_timestamp = _timestamp;
        copy->_has.timestamp = YES;
    }
    if (self->_has.reason)
    {
        copy->_reason = _reason;
        copy->_has.reason = YES;
    }
    if (self->_has.sqlitecode)
    {
        copy->_sqlitecode = _sqlitecode;
        copy->_has.sqlitecode = YES;
    }
    return copy;
}

- (BOOL)isEqual:(id)object
{
    AWDKeychainSecDbMarkedCorrupt *other = (AWDKeychainSecDbMarkedCorrupt *)object;
    return [other isMemberOfClass:[self class]]
    &&
    ((self->_has.timestamp && other->_has.timestamp && self->_timestamp == other->_timestamp) || (!self->_has.timestamp && !other->_has.timestamp))
    &&
    ((self->_has.reason && other->_has.reason && self->_reason == other->_reason) || (!self->_has.reason && !other->_has.reason))
    &&
    ((self->_has.sqlitecode && other->_has.sqlitecode && self->_sqlitecode == other->_sqlitecode) || (!self->_has.sqlitecode && !other->_has.sqlitecode))
    ;
}

- (NSUInteger)hash
{
    return 0
    ^
    (self->_has.timestamp ? PBHashInt((NSUInteger)self->_timestamp) : 0)
    ^
    (self->_has.reason ? PBHashInt((NSUInteger)self->_reason) : 0)
    ^
    (self->_has.sqlitecode ? PBHashInt((NSUInteger)self->_sqlitecode) : 0)
    ;
}

- (void)mergeFrom:(AWDKeychainSecDbMarkedCorrupt *)other
{
    if (other->_has.timestamp)
    {
        self->_timestamp = other->_timestamp;
        self->_has.timestamp = YES;
    }
    if (other->_has.reason)
    {
        self->_reason = other->_reason;
        self->_has.reason = YES;
    }
    if (other->_has.sqlitecode)
    {
        self->_sqlitecode = other->_sqlitecode;
        self->_has.sqlitecode = YES;
    }
}

@end
#endif