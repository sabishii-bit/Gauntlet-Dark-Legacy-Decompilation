#ifndef _WINDOWS_KERNEL_H_
#define _WINDOWS_KERNEL_H_

// Category: windows_kernel
// Extracted from Xbox PDB symbols
// Total types: 392
// Note: Xbox symbols - may need adaptation for GameCube

enum _KEY_INFORMATION_CLASS
{
    KeyBasicInformation=0,
    KeyNodeInformation=1,
    KeyFullInformation=2,
    KeyNameInformation=3
};

enum _KEY_SET_INFORMATION_CLASS
{
    KeyWriteTimeInformation=0
};

enum _PORT_INFORMATION_CLASS
{
    PortBasicInformation=0,
    PortDumpInformation=1
};

enum _KEY_VALUE_INFORMATION_CLASS
{
    KeyValueBasicInformation=0,
    KeyValueFullInformation=1,
    KeyValuePartialInformation=2,
    KeyValueFullInformationAlign64=3,
    KeyValuePartialInformationAlign64=4
};

enum _FILE_INFORMATION_CLASS
{
    FileDirectoryInformation=1,
    FileFullDirectoryInformation=2,
    FileBothDirectoryInformation=3,
    FileBasicInformation=4,
    FileStandardInformation=5,
    FileInternalInformation=6,
    FileEaInformation=7,
    FileAccessInformation=8,
    FileNameInformation=9,
    FileRenameInformation=10,
    FileLinkInformation=11,
    FileNamesInformation=12,
    FileDispositionInformation=13,
    FilePositionInformation=14,
    FileFullEaInformation=15,
    FileModeInformation=16,
    FileAlignmentInformation=17,
    FileAllInformation=18,
    FileAllocationInformation=19,
    FileEndOfFileInformation=20,
    FileAlternateNameInformation=21,
    FileStreamInformation=22,
    FilePipeInformation=23,
    FilePipeLocalInformation=24,
    FilePipeRemoteInformation=25,
    FileMailslotQueryInformation=26,
    FileMailslotSetInformation=27,
    FileCompressionInformation=28,
    FileObjectIdInformation=29,
    FileCompletionInformation=30,
    FileMoveClusterInformation=31,
    FileQuotaInformation=32,
    FileReparsePointInformation=33,
    FileNetworkOpenInformation=34,
    FileAttributeTagInformation=35,
    FileTrackingInformation=36,
    FileMaximumInformation=37
};

enum _NT_PRODUCT_TYPE
{
    NtProductWinNt=1,
    NtProductLanManNt=2,
    NtProductServer=3
};

enum _KPROFILE_SOURCE
{
    ProfileTime=0,
    ProfileAlignmentFixup=1,
    ProfileTotalIssues=2,
    ProfilePipelineDry=3,
    ProfileLoadInstructions=4,
    ProfilePipelineFrozen=5,
    ProfileBranchInstructions=6,
    ProfileTotalNonissues=7,
    ProfileDcacheMisses=8,
    ProfileIcacheMisses=9,
    ProfileCacheMisses=10,
    ProfileBranchMispredictions=11,
    ProfileStoreInstructions=12,
    ProfileFpInstructions=13,
    ProfileIntegerInstructions=14,
    Profile2Issue=15,
    Profile3Issue=16,
    Profile4Issue=17,
    ProfileSpecialInstructions=18,
    ProfileTotalCycles=19,
    ProfileIcacheIssues=20,
    ProfileDcacheAccesses=21,
    ProfileMemoryBarrierCycles=22,
    ProfileLoadLinkedIssues=23,
    ProfileMaximum=24
};

enum _CM_SERVICE_NODE_TYPE
{
    DriverType=1,
    FileSystemType=2,
    Win32ServiceOwnProcess=16,
    Win32ServiceShareProcess=32,
    AdapterType=4,
    RecognizerType=8
};

enum _SYSTEM_INFORMATION_CLASS
{
    SystemBasicInformation=0,
    SystemProcessorInformation=1,
    SystemPerformanceInformation=2,
    SystemTimeOfDayInformation=3,
    SystemPathInformation=4,
    SystemProcessInformation=5,
    SystemCallCountInformation=6,
    SystemDeviceInformation=7,
    SystemProcessorPerformanceInformation=8,
    SystemFlagsInformation=9,
    SystemCallTimeInformation=10,
    SystemModuleInformation=11,
    SystemLocksInformation=12,
    SystemStackTraceInformation=13,
    SystemPagedPoolInformation=14,
    SystemNonPagedPoolInformation=15,
    SystemHandleInformation=16,
    SystemObjectInformation=17,
    SystemPageFileInformation=18,
    SystemVdmInstemulInformation=19,
    SystemVdmBopInformation=20,
    SystemFileCacheInformation=21,
    SystemPoolTagInformation=22,
    SystemInterruptInformation=23,
    SystemDpcBehaviorInformation=24,
    SystemFullMemoryInformation=25,
    SystemLoadGdiDriverInformation=26,
    SystemUnloadGdiDriverInformation=27,
    SystemTimeAdjustmentInformation=28,
    SystemSummaryMemoryInformation=29,
    SystemUnused1=30,
    SystemPerformanceTraceInformation=31,
    SystemCrashDumpInformation=32,
    SystemExceptionInformation=33,
    SystemCrashDumpStateInformation=34,
    SystemKernelDebuggerInformation=35,
    SystemContextSwitchInformation=36,
    SystemRegistryQuotaInformation=37,
    SystemExtendServiceTableInformation=38,
    SystemPrioritySeperation=39,
    SystemUnused3=40,
    SystemUnused4=41,
    SystemUnused5=42,
    SystemLegacyDriverInformation=43,
    SystemCurrentTimeZoneInformation=44,
    SystemLookasideInformation=45,
    SystemTimeSlipNotification=46,
    SystemSessionCreate=47,
    SystemSessionDetach=48,
    SystemSessionInformation=49,
    SystemRangeStartInformation=50,
    SystemVerifierInformation=51,
    SystemVerifierThunkExtend=52,
    SystemSessionProcessInformation=53
};

enum _CM_SERVICE_LOAD_TYPE
{
    BootLoad=0,
    SystemLoad=1,
    AutoLoad=2,
    DemandLoad=3,
    DisableLoad=4
};

enum _CM_ERROR_CONTROL_TYPE
{
    IgnoreError=0,
    NormalError=1,
    SevereError=2,
    CriticalError=3
};

enum _CM_SHARE_DISPOSITION
{
    CmResourceShareUndetermined=0,
    CmResourceShareDeviceExclusive=1,
    CmResourceShareDriverExclusive=2,
    CmResourceShareShared=3
};

enum _PROCESSINFOCLASS
{
    ProcessBasicInformation=0,
    ProcessQuotaLimits=1,
    ProcessIoCounters=2,
    ProcessVmCounters=3,
    ProcessTimes=4,
    ProcessBasePriority=5,
    ProcessRaisePriority=6,
    ProcessDebugPort=7,
    ProcessExceptionPort=8,
    ProcessAccessToken=9,
    ProcessLdtInformation=10,
    ProcessLdtSize=11,
    ProcessDefaultHardErrorMode=12,
    ProcessIoPortHandlers=13,
    ProcessPooledUsageAndLimits=14,
    ProcessWorkingSetWatch=15,
    ProcessUserModeIOPL=16,
    ProcessEnableAlignmentFaultFixup=17,
    ProcessPriorityClass=18,
    ProcessWx86Information=19,
    ProcessHandleCount=20,
    ProcessAffinityMask=21,
    ProcessPriorityBoost=22,
    ProcessDeviceMap=23,
    ProcessSessionInformation=24,
    ProcessForegroundInformation=25,
    ProcessWow64Information=26,
    MaxProcessInfoClass=27
};

enum _THREADINFOCLASS
{
    ThreadBasicInformation=0,
    ThreadTimes=1,
    ThreadPriority=2,
    ThreadBasePriority=3,
    ThreadAffinityMask=4,
    ThreadImpersonationToken=5,
    ThreadDescriptorTableEntry=6,
    ThreadEnableAlignmentFaultFixup=7,
    ThreadEventPair_Reusable=8,
    ThreadQuerySetWin32StartAddress=9,
    ThreadZeroTlsCell=10,
    ThreadPerformanceCount=11,
    ThreadAmILastThread=12,
    ThreadIdealProcessor=13,
    ThreadPriorityBoost=14,
    ThreadSetTlsArrayAddress=15,
    ThreadIsIoPending=16,
    ThreadHideFromDebugger=17,
    MaxThreadInfoClass=18
};

enum _RTL_PATH_TYPE
{
    RtlPathTypeUnknown=0,
    RtlPathTypeUncAbsolute=1,
    RtlPathTypeDriveAbsolute=2,
    RtlPathTypeDriveRelative=3,
    RtlPathTypeRooted=4,
    RtlPathTypeRelative=5,
    RtlPathTypeLocalDevice=6,
    RtlPathTypeRootLocalDevice=7
};

enum _SID_NAME_USE
{
    SidTypeUser=1,
    SidTypeGroup=2,
    SidTypeDomain=3,
    SidTypeAlias=4,
    SidTypeWellKnownGroup=5,
    SidTypeDeletedAccount=6,
    SidTypeInvalid=7,
    SidTypeUnknown=8,
    SidTypeComputer=9
};

enum _ACL_INFORMATION_CLASS
{
    AclRevisionInformation=1,
    AclSizeInformation=2
};

enum _AUDIT_EVENT_TYPE
{
    AuditEventObjectAccess=0,
    AuditEventDirectoryServiceAccess=1
};

enum _SECURITY_IMPERSONATION_LEVEL
{
    SecurityAnonymous=0,
    SecurityIdentification=1,
    SecurityImpersonation=2,
    SecurityDelegation=3
};

enum _TOKEN_TYPE
{
    TokenPrimary=1,
    TokenImpersonation=2
};

enum _TOKEN_INFORMATION_CLASS
{
    TokenUser=1,
    TokenGroups=2,
    TokenPrivileges=3,
    TokenOwner=4,
    TokenPrimaryGroup=5,
    TokenDefaultDacl=6,
    TokenSource=7,
    TokenType=8,
    TokenImpersonationLevel=9,
    TokenStatistics=10,
    TokenRestrictedSids=11,
    TokenSessionId=12
};

enum _KOBJECTS
{
    EventNotificationObject=0,
    EventSynchronizationObject=1,
    MutantObject=2,
    ProcessObject=3,
    QueueObject=4,
    SemaphoreObject=5,
    ThreadObject=6,
    Spare1Object=7,
    TimerNotificationObject=8,
    TimerSynchronizationObject=9,
    Spare2Object=10,
    Spare3Object=11,
    Spare4Object=12,
    Spare5Object=13,
    Spare6Object=14,
    Spare7Object=15,
    Spare8Object=16,
    Spare9Object=17,
    ApcObject=18,
    DpcObject=19,
    DeviceQueueObject=20,
    EventPairObject=21,
    InterruptObject=22,
    ProfileObject=23
};

enum _KINTERRUPT_MODE
{
    LevelSensitive=0,
    Latched=1
};

enum _KTHREAD_STATE
{
    Initialized=0,
    Ready=1,
    Running=2,
    Standby=3,
    Terminated=4,
    Waiting=5,
    Transition=6
};

enum _SYSTEM_POWER_STATE
{
    PowerSystemUnspecified=0,
    PowerSystemWorking=1,
    PowerSystemSleeping1=2,
    PowerSystemSleeping2=3,
    PowerSystemSleeping3=4,
    PowerSystemHibernate=5,
    PowerSystemShutdown=6,
    PowerSystemMaximum=7
};

enum _KWAIT_REASON
{
    Executive=0,
    FreePage=1,
    PageIn=2,
    PoolAllocation=3,
    DelayExecution=4,
    Suspended=5,
    UserRequest=6,
    WrExecutive=7,
    WrFreePage=8,
    WrPageIn=9,
    WrPoolAllocation=10,
    WrDelayExecution=11,
    WrSuspended=12,
    WrUserRequest=13,
    WrEventPair=14,
    WrQueue=15,
    WrLpcReceive=16,
    WrLpcReply=17,
    WrVirtualMemory=18,
    WrPageOut=19,
    WrRendezvous=20,
    WrFsCacheIn=21,
    WrFsCacheOut=22,
    Spare4=23,
    Spare5=24,
    Spare6=25,
    WrKernel=26,
    MaximumWaitReason=27
};

enum _DEVICE_POWER_STATE
{
    PowerDeviceUnspecified=0,
    PowerDeviceD0=1,
    PowerDeviceD1=2,
    PowerDeviceD2=3,
    PowerDeviceD3=4,
    PowerDeviceMaximum=5
};

enum _POWER_STATE_TYPE
{
    SystemPowerState=0,
    DevicePowerState=1
};

enum _RTL_GENERIC_COMPARE_RESULTS
{
    GenericLessThan=0,
    GenericGreaterThan=1,
    GenericEqual=2
};

enum _SYSDBG_COMMAND
{
    SysDbgQueryModuleInformation=0,
    SysDbgQueryTraceInformation=1,
    SysDbgSetTracepoint=2,
    SysDbgSetSpecialCall=3,
    SysDbgClearSpecialCalls=4,
    SysDbgQuerySpecialCalls=5,
    SysDbgBreakPoint=6
};

enum _EVENT_TYPE
{
    NotificationEvent=0,
    SynchronizationEvent=1
};

enum _HARDERROR_RESPONSE_OPTION
{
    OptionAbortRetryIgnore=0,
    OptionOk=1,
    OptionOkCancel=2,
    OptionRetryCancel=3,
    OptionYesNo=4,
    OptionYesNoCancel=5,
    OptionShutdownSystem=6,
    OptionOkNoWait=7,
    OptionCancelTryContinue=8
};

enum _TIMER_TYPE
{
    NotificationTimer=0,
    SynchronizationTimer=1
};

enum _WAIT_TYPE
{
    WaitAll=0,
    WaitAny=1
};

enum _MODE
{
    KernelMode=0,
    UserMode=1,
    MaximumMode=2
};

enum _HARDERROR_RESPONSE
{
    ResponseReturnToCaller=0,
    ResponseNotHandled=1,
    ResponseAbort=2,
    ResponseCancel=3,
    ResponseIgnore=4,
    ResponseNo=5,
    ResponseOk=6,
    ResponseRetry=7,
    ResponseYes=8,
    ResponseTryAgain=9,
    ResponseContinue=10
};

enum IMPORT_OBJECT_TYPE
{
    IMPORT_OBJECT_CODE=0,
    IMPORT_OBJECT_DATA=1,
    IMPORT_OBJECT_CONST=2
};

enum IMPORT_OBJECT_NAME_TYPE
{
    IMPORT_OBJECT_ORDINAL=0,
    IMPORT_OBJECT_NAME=1,
    IMPORT_OBJECT_NAME_NO_PREFIX=2,
    IMPORT_OBJECT_NAME_UNDECORATE=3
};

enum _SHUTDOWN_ACTION
{
    ShutdownNoReboot=0,
    ShutdownReboot=1,
    ShutdownPowerOff=2
};

enum _SECTION_INHERIT
{
    ViewShare=1,
    ViewUnmap=2
};

struct _PROCESSOR_POWER_STATE// Size=0x88 (Id=142)
{
    void  ( * IdleFunction)(struct _PROCESSOR_POWER_STATE * );// Offset=0x0 Size=0x4
    unsigned long Idle0KernelTimeLimit;// Offset=0x4 Size=0x4
    unsigned long Idle0LastTime;// Offset=0x8 Size=0x4
    void * IdleState;// Offset=0xc Size=0x4
    unsigned long long LastCheck;// Offset=0x10 Size=0x8
    struct PROCESSOR_IDLE_TIMES IdleTimes;// Offset=0x18 Size=0x20
    unsigned long IdleTime1;// Offset=0x38 Size=0x4
    unsigned long PromotionCheck;// Offset=0x3c Size=0x4
    unsigned long IdleTime2;// Offset=0x40 Size=0x4
    unsigned char CurrentThrottle;// Offset=0x44 Size=0x1
    unsigned char ThrottleLimit;// Offset=0x45 Size=0x1
    unsigned char Spare1[2];// Offset=0x46 Size=0x2
    unsigned long SetMember;// Offset=0x48 Size=0x4
    void * AbortThrottle;// Offset=0x4c Size=0x4
    unsigned long long DebugDelta;// Offset=0x50 Size=0x8
    unsigned long DebugCount;// Offset=0x58 Size=0x4
    unsigned long LastSysTime;// Offset=0x5c Size=0x4
    unsigned long Spare2[10];// Offset=0x60 Size=0x28
};

struct _CM_DISK_GEOMETRY_DEVICE_DATA// Size=0x10 (Id=144)
{
    unsigned long BytesPerSector;// Offset=0x0 Size=0x4
    unsigned long NumberOfCylinders;// Offset=0x4 Size=0x4
    unsigned long SectorsPerTrack;// Offset=0x8 Size=0x4
    unsigned long NumberOfHeads;// Offset=0xc Size=0x4
};

struct _RTL_SPLAY_LINKS// Size=0xc (Id=147)
{
    struct _RTL_SPLAY_LINKS * Parent;// Offset=0x0 Size=0x4
    struct _RTL_SPLAY_LINKS * LeftChild;// Offset=0x4 Size=0x4
    struct _RTL_SPLAY_LINKS * RightChild;// Offset=0x8 Size=0x4
};

struct _IMAGE_IA64_RUNTIME_FUNCTION_ENTRY// Size=0xc (Id=153)
{
    unsigned long BeginAddress;// Offset=0x0 Size=0x4
    unsigned long EndAddress;// Offset=0x4 Size=0x4
    unsigned long UnwindInfoAddress;// Offset=0x8 Size=0x4
};

struct _PROCESS_HEAP_ENTRY// Size=0x1c (Id=156)
{
    void * lpData;// Offset=0x0 Size=0x4
    unsigned long cbData;// Offset=0x4 Size=0x4
    unsigned char cbOverhead;// Offset=0x8 Size=0x1
    unsigned char iRegionIndex;// Offset=0x9 Size=0x1
    unsigned short wFlags;// Offset=0xa Size=0x2
    union // Size=0x10 (Id=0)
    {
        struct __unnamed Block;// Offset=0xc Size=0x10
        struct __unnamed Region;// Offset=0xc Size=0x10
    };
};

struct _SID_AND_ATTRIBUTES// Size=0x8 (Id=159)
{
    void * Sid;// Offset=0x0 Size=0x4
    unsigned long Attributes;// Offset=0x4 Size=0x4
};

struct _SYSTEM_THREAD_INFORMATION// Size=0x40 (Id=160)
{
    union _LARGE_INTEGER KernelTime;// Offset=0x0 Size=0x8
    union _LARGE_INTEGER UserTime;// Offset=0x8 Size=0x8
    union _LARGE_INTEGER CreateTime;// Offset=0x10 Size=0x8
    unsigned long WaitTime;// Offset=0x18 Size=0x4
    void * StartAddress;// Offset=0x1c Size=0x4
    struct _CLIENT_ID ClientId;// Offset=0x20 Size=0x8
    long Priority;// Offset=0x28 Size=0x4
    long BasePriority;// Offset=0x2c Size=0x4
    unsigned long ContextSwitches;// Offset=0x30 Size=0x4
    unsigned long ThreadState;// Offset=0x34 Size=0x4
    unsigned long WaitReason;// Offset=0x38 Size=0x4
};

struct _SYSTEM_CRASH_DUMP_INFORMATION// Size=0x8 (Id=161)
{
    void * CrashDumpSection;// Offset=0x0 Size=0x4
    void * hDumpSection;// Offset=0x4 Size=0x4
};

struct _FILE_FS_VOLUME_INFORMATION// Size=0x18 (Id=164)
{
    union _LARGE_INTEGER VolumeCreationTime;// Offset=0x0 Size=0x8
    unsigned long VolumeSerialNumber;// Offset=0x8 Size=0x4
    unsigned long VolumeLabelLength;// Offset=0xc Size=0x4
    unsigned char SupportsObjects;// Offset=0x10 Size=0x1
    char VolumeLabel[1];// Offset=0x11 Size=0x1
};

struct _SYSTEM_HANDLE_TABLE_ENTRY_INFO// Size=0x10 (Id=165)
{
    unsigned short UniqueProcessId;// Offset=0x0 Size=0x2
    unsigned short CreatorBackTraceIndex;// Offset=0x2 Size=0x2
    unsigned char ObjectTypeIndex;// Offset=0x4 Size=0x1
    unsigned char HandleAttributes;// Offset=0x5 Size=0x1
    unsigned short HandleValue;// Offset=0x6 Size=0x2
    void * Object;// Offset=0x8 Size=0x4
    unsigned long GrantedAccess;// Offset=0xc Size=0x4
};

struct _PROCESS_FOREGROUND_BACKGROUND// Size=0x1 (Id=166)
{
    unsigned char Foreground;// Offset=0x0 Size=0x1
};

struct _IO_RESOURCE_DESCRIPTOR// Size=0x20 (Id=177)
{
    unsigned char Option;// Offset=0x0 Size=0x1
    unsigned char Type;// Offset=0x1 Size=0x1
    unsigned char ShareDisposition;// Offset=0x2 Size=0x1
    unsigned char Spare1;// Offset=0x3 Size=0x1
    unsigned short Flags;// Offset=0x4 Size=0x2
    unsigned short Spare2;// Offset=0x6 Size=0x2
    union __unnamed u;// Offset=0x8 Size=0x18
};

struct _FILE_MAILSLOT_QUERY_INFORMATION// Size=0x18 (Id=179)
{
    unsigned long MaximumMessageSize;// Offset=0x0 Size=0x4
    unsigned long MailslotQuota;// Offset=0x4 Size=0x4
    unsigned long NextMessageSize;// Offset=0x8 Size=0x4
    unsigned long MessagesAvailable;// Offset=0xc Size=0x4
    union _LARGE_INTEGER ReadTimeout;// Offset=0x10 Size=0x8
};

struct _SYSTEM_POOLTAG_INFORMATION// Size=0x20 (Id=180)
{
    unsigned long Count;// Offset=0x0 Size=0x4
    struct _SYSTEM_POOLTAG TagInfo[1];// Offset=0x4 Size=0x1c
};

struct _TOKEN_CONTROL// Size=0x28 (Id=182)
{
    struct _LUID TokenId;// Offset=0x0 Size=0x8
    struct _LUID AuthenticationId;// Offset=0x8 Size=0x8
    struct _LUID ModifiedId;// Offset=0x10 Size=0x8
    struct _TOKEN_SOURCE TokenSource;// Offset=0x18 Size=0x10
};

struct _KINTERRUPT// Size=0x70 (Id=190)
{
    unsigned char  ( * ServiceRoutine)(struct _KINTERRUPT * ,void * );// Offset=0x0 Size=0x4
    void * ServiceContext;// Offset=0x4 Size=0x4
    unsigned long BusInterruptLevel;// Offset=0x8 Size=0x4
    unsigned long Irql;// Offset=0xc Size=0x4
    unsigned char Connected;// Offset=0x10 Size=0x1
    unsigned char ShareVector;// Offset=0x11 Size=0x1
    unsigned char Mode;// Offset=0x12 Size=0x1
    unsigned char __align0[1];// Offset=0x13 Size=0x1
    unsigned long ServiceCount;// Offset=0x14 Size=0x4
    unsigned long DispatchCode[22];// Offset=0x18 Size=0x58
};

struct _ACE_HEADER// Size=0x4 (Id=196)
{
    unsigned char AceType;// Offset=0x0 Size=0x1
    unsigned char AceFlags;// Offset=0x1 Size=0x1
    unsigned short AceSize;// Offset=0x2 Size=0x2
};

struct _THREAD_BASIC_INFORMATION// Size=0x1c (Id=197)
{
    long ExitStatus;// Offset=0x0 Size=0x4
    struct _TEB * TebBaseAddress;// Offset=0x4 Size=0x4
    struct _CLIENT_ID ClientId;// Offset=0x8 Size=0x8
    unsigned long AffinityMask;// Offset=0x10 Size=0x4
    long Priority;// Offset=0x14 Size=0x4
    long BasePriority;// Offset=0x18 Size=0x4
};

struct _SYSTEM_OBJECTTYPE_INFORMATION// Size=0x38 (Id=207)
{
    unsigned long NextEntryOffset;// Offset=0x0 Size=0x4
    unsigned long NumberOfObjects;// Offset=0x4 Size=0x4
    unsigned long NumberOfHandles;// Offset=0x8 Size=0x4
    unsigned long TypeIndex;// Offset=0xc Size=0x4
    unsigned long InvalidAttributes;// Offset=0x10 Size=0x4
    struct _GENERIC_MAPPING GenericMapping;// Offset=0x14 Size=0x10
    unsigned long ValidAccessMask;// Offset=0x24 Size=0x4
    unsigned long PoolType;// Offset=0x28 Size=0x4
    unsigned char SecurityRequired;// Offset=0x2c Size=0x1
    unsigned char WaitableObject;// Offset=0x2d Size=0x1
    unsigned char __align0[2];// Offset=0x2e Size=0x2
    struct _STRING TypeName;// Offset=0x30 Size=0x8
};

struct _RTL_HEAP_ENTRY// Size=0x10 (Id=211)
{
    unsigned long Size;// Offset=0x0 Size=0x4
    unsigned short Flags;// Offset=0x4 Size=0x2
    unsigned short AllocatorBackTraceIndex;// Offset=0x6 Size=0x2
    union __unnamed u;// Offset=0x8 Size=0x8
};

struct _KMUTANT// Size=0x20 (Id=213)
{
    struct _DISPATCHER_HEADER Header;// Offset=0x0 Size=0x10
    struct _LIST_ENTRY MutantListEntry;// Offset=0x10 Size=0x8
    struct _KTHREAD * OwnerThread;// Offset=0x18 Size=0x4
    unsigned char Abandoned;// Offset=0x1c Size=0x1
};

struct _KEVENT// Size=0x10 (Id=220)
{
    struct _DISPATCHER_HEADER Header;// Offset=0x0 Size=0x10
};

struct _SYSTEM_SET_TIME_ADJUST_INFORMATION// Size=0x8 (Id=221)
{
    unsigned long TimeAdjustment;// Offset=0x0 Size=0x4
    unsigned char Enable;// Offset=0x4 Size=0x1
};

struct _IMAGE_OPTIONAL_HEADER64// Size=0xf0 (Id=226)
{
    unsigned short Magic;// Offset=0x0 Size=0x2
    unsigned char MajorLinkerVersion;// Offset=0x2 Size=0x1
    unsigned char MinorLinkerVersion;// Offset=0x3 Size=0x1
    unsigned long SizeOfCode;// Offset=0x4 Size=0x4
    unsigned long SizeOfInitializedData;// Offset=0x8 Size=0x4
    unsigned long SizeOfUninitializedData;// Offset=0xc Size=0x4
    unsigned long AddressOfEntryPoint;// Offset=0x10 Size=0x4
    unsigned long BaseOfCode;// Offset=0x14 Size=0x4
    unsigned long long ImageBase;// Offset=0x18 Size=0x8
    unsigned long SectionAlignment;// Offset=0x20 Size=0x4
    unsigned long FileAlignment;// Offset=0x24 Size=0x4
    unsigned short MajorOperatingSystemVersion;// Offset=0x28 Size=0x2
    unsigned short MinorOperatingSystemVersion;// Offset=0x2a Size=0x2
    unsigned short MajorImageVersion;// Offset=0x2c Size=0x2
    unsigned short MinorImageVersion;// Offset=0x2e Size=0x2
    unsigned short MajorSubsystemVersion;// Offset=0x30 Size=0x2
    unsigned short MinorSubsystemVersion;// Offset=0x32 Size=0x2
    unsigned long Win32VersionValue;// Offset=0x34 Size=0x4
    unsigned long SizeOfImage;// Offset=0x38 Size=0x4
    unsigned long SizeOfHeaders;// Offset=0x3c Size=0x4
    unsigned long CheckSum;// Offset=0x40 Size=0x4
    unsigned short Subsystem;// Offset=0x44 Size=0x2
    unsigned short DllCharacteristics;// Offset=0x46 Size=0x2
    unsigned long long SizeOfStackReserve;// Offset=0x48 Size=0x8
    unsigned long long SizeOfStackCommit;// Offset=0x50 Size=0x8
    unsigned long long SizeOfHeapReserve;// Offset=0x58 Size=0x8
    unsigned long long SizeOfHeapCommit;// Offset=0x60 Size=0x8
    unsigned long LoaderFlags;// Offset=0x68 Size=0x4
    unsigned long NumberOfRvaAndSizes;// Offset=0x6c Size=0x4
    struct _IMAGE_DATA_DIRECTORY DataDirectory[16];// Offset=0x70 Size=0x80
};

struct _KTIMER// Size=0x28 (Id=228)
{
    struct _DISPATCHER_HEADER Header;// Offset=0x0 Size=0x10
    union _ULARGE_INTEGER DueTime;// Offset=0x10 Size=0x8
    struct _LIST_ENTRY TimerListEntry;// Offset=0x18 Size=0x8
    struct _KDPC * Dpc;// Offset=0x20 Size=0x4
    long Period;// Offset=0x24 Size=0x4
};

union _IMAGE_AUX_SYMBOL// Size=0x12 (Id=238)
{
    union // Size=0x12 (Id=0)
    {
        struct __unnamed Sym;// Offset=0x0 Size=0x12
        struct __unnamed File;// Offset=0x0 Size=0x12
        struct __unnamed Section;// Offset=0x0 Size=0x10
    };
};

struct _FILE_PIPE_LOCAL_INFORMATION// Size=0x28 (Id=239)
{
    unsigned long NamedPipeType;// Offset=0x0 Size=0x4
    unsigned long NamedPipeConfiguration;// Offset=0x4 Size=0x4
    unsigned long MaximumInstances;// Offset=0x8 Size=0x4
    unsigned long CurrentInstances;// Offset=0xc Size=0x4
    unsigned long InboundQuota;// Offset=0x10 Size=0x4
    unsigned long ReadDataAvailable;// Offset=0x14 Size=0x4
    unsigned long OutboundQuota;// Offset=0x18 Size=0x4
    unsigned long WriteQuotaAvailable;// Offset=0x1c Size=0x4
    unsigned long NamedPipeState;// Offset=0x20 Size=0x4
    unsigned long NamedPipeEnd;// Offset=0x24 Size=0x4
};

struct _KDPC// Size=0x1c (Id=242)
{
    short Type;// Offset=0x0 Size=0x2
    unsigned char Inserted;// Offset=0x2 Size=0x1
    unsigned char Padding;// Offset=0x3 Size=0x1
    struct _LIST_ENTRY DpcListEntry;// Offset=0x4 Size=0x8
    void  ( * DeferredRoutine)(struct _KDPC * ,void * ,void * ,void * );// Offset=0xc Size=0x4
    void * DeferredContext;// Offset=0x10 Size=0x4
    void * SystemArgument1;// Offset=0x14 Size=0x4
    void * SystemArgument2;// Offset=0x18 Size=0x4
};

struct _KDEVICE_QUEUE// Size=0xc (Id=243)
{
    short Type;// Offset=0x0 Size=0x2
    unsigned char Size;// Offset=0x2 Size=0x1
    unsigned char Busy;// Offset=0x3 Size=0x1
    struct _LIST_ENTRY DeviceListHead;// Offset=0x4 Size=0x8
};

struct _RTL_BITMAP// Size=0x8 (Id=247)
{
    unsigned long SizeOfBitMap;// Offset=0x0 Size=0x4
    unsigned long * Buffer;// Offset=0x4 Size=0x4
};

struct _FILE_REPARSE_POINT_INFORMATION// Size=0x10 (Id=249)
{
    long long FileReference;// Offset=0x0 Size=0x8
    unsigned long Tag;// Offset=0x8 Size=0x4
};

struct _CM_PNP_BIOS_INSTALLATION_CHECK// Size=0x21 (Id=252)
{
    unsigned char Signature[4];// Offset=0x0 Size=0x4
    unsigned char Revision;// Offset=0x4 Size=0x1
    unsigned char Length;// Offset=0x5 Size=0x1
    unsigned short ControlField;// Offset=0x6 Size=0x2
    unsigned char Checksum;// Offset=0x8 Size=0x1
    unsigned long EventFlagAddress;// Offset=0x9 Size=0x4
    unsigned short RealModeEntryOffset;// Offset=0xd Size=0x2
    unsigned short RealModeEntrySegment;// Offset=0xf Size=0x2
    unsigned short ProtectedModeEntryOffset;// Offset=0x11 Size=0x2
    unsigned long ProtectedModeCodeBaseAddress;// Offset=0x13 Size=0x4
    unsigned long OemDeviceId;// Offset=0x17 Size=0x4
    unsigned short RealModeDataBaseAddress;// Offset=0x1b Size=0x2
    unsigned long ProtectedModeDataBaseAddress;// Offset=0x1d Size=0x4
};

struct _IMAGE_RESOURCE_DIRECTORY_ENTRY// Size=0x8 (Id=253)
{
    union // Size=0x4 (Id=0)
    {
        struct // Size=0x4 (Id=0)
        {
            unsigned long NameOffset:31;// Offset=0x0 Size=0x4 BitOffset=0x0 BitSize=0x1f
            unsigned long NameIsString:1;// Offset=0x0 Size=0x4 BitOffset=0x1f BitSize=0x1
        };
        unsigned long Name;// Offset=0x0 Size=0x4
        unsigned short Id;// Offset=0x0 Size=0x2
        unsigned char __align0[2];// Offset=0x2 Size=0x2
    };
    union // Size=0x4 (Id=0)
    {
        unsigned long OffsetToData;// Offset=0x4 Size=0x4
        struct // Size=0x4 (Id=0)
        {
            unsigned long OffsetToDirectory:31;// Offset=0x4 Size=0x4 BitOffset=0x0 BitSize=0x1f
            unsigned long DataIsDirectory:1;// Offset=0x4 Size=0x4 BitOffset=0x1f BitSize=0x1
        };
    };
};

struct _IMAGE_NT_HEADERS// Size=0xf8 (Id=256)
{
    unsigned long Signature;// Offset=0x0 Size=0x4
    struct _IMAGE_FILE_HEADER FileHeader;// Offset=0x4 Size=0x14
    struct _IMAGE_OPTIONAL_HEADER OptionalHeader;// Offset=0x18 Size=0xe0
};

struct _TOKEN_DEFAULT_DACL// Size=0x4 (Id=271)
{
    struct _ACL * DefaultDacl;// Offset=0x0 Size=0x4
};

struct _FILE_FS_CONTROL_INFORMATION// Size=0x30 (Id=272)
{
    union _LARGE_INTEGER FreeSpaceStartFiltering;// Offset=0x0 Size=0x8
    union _LARGE_INTEGER FreeSpaceThreshold;// Offset=0x8 Size=0x8
    union _LARGE_INTEGER FreeSpaceStopFiltering;// Offset=0x10 Size=0x8
    union _LARGE_INTEGER DefaultQuotaThreshold;// Offset=0x18 Size=0x8
    union _LARGE_INTEGER DefaultQuotaLimit;// Offset=0x20 Size=0x8
    unsigned long FileSystemControlFlags;// Offset=0x28 Size=0x4
};

struct _KPCR// Size=0x284 (Id=273)
{
    struct _NT_TIB NtTib;// Offset=0x0 Size=0x1c
    struct _KPCR * SelfPcr;// Offset=0x1c Size=0x4
    struct _KPRCB * Prcb;// Offset=0x20 Size=0x4
    unsigned char Irql;// Offset=0x24 Size=0x1
    unsigned char __align0[3];// Offset=0x25 Size=0x3
    struct _KPRCB PrcbData;// Offset=0x28 Size=0x25c
};

struct _RTL_PERTHREAD_CURDIR// Size=0xc (Id=276)
{
    struct _RTL_DRIVE_LETTER_CURDIR * CurrentDirectories;// Offset=0x0 Size=0x4
    struct _UNICODE_STRING * ImageName;// Offset=0x4 Size=0x4
    void * Environment;// Offset=0x8 Size=0x4
};

struct _IO_COMPLETION_BASIC_INFORMATION// Size=0x4 (Id=277)
{
    long Depth;// Offset=0x0 Size=0x4
};

struct _IMAGE_FUNCTION_ENTRY64// Size=0x18 (Id=285)
{
    unsigned long long StartingAddress;// Offset=0x0 Size=0x8
    unsigned long long EndingAddress;// Offset=0x8 Size=0x8
    union // Size=0x8 (Id=0)
    {
        unsigned long long EndOfPrologue;// Offset=0x10 Size=0x8
        unsigned long long UnwindInfoAddress;// Offset=0x10 Size=0x8
    };
};

struct _KAPC// Size=0x28 (Id=288)
{
    short Type;// Offset=0x0 Size=0x2
    char ApcMode;// Offset=0x2 Size=0x1
    unsigned char Inserted;// Offset=0x3 Size=0x1
    struct _KTHREAD * Thread;// Offset=0x4 Size=0x4
    struct _LIST_ENTRY ApcListEntry;// Offset=0x8 Size=0x8
    void  ( * KernelRoutine)(struct _KAPC * ,void  ( ** )(void * ,void * ,void * ),void ** ,void ** ,void ** );// Offset=0x10 Size=0x4
    void  ( * RundownRoutine)(struct _KAPC * );// Offset=0x14 Size=0x4
    void  ( * NormalRoutine)(void * ,void * ,void * );// Offset=0x18 Size=0x4
    void * NormalContext;// Offset=0x1c Size=0x4
    void * SystemArgument1;// Offset=0x20 Size=0x4
    void * SystemArgument2;// Offset=0x24 Size=0x4
};

struct _KSEMAPHORE// Size=0x14 (Id=289)
{
    struct _DISPATCHER_HEADER Header;// Offset=0x0 Size=0x10
    long Limit;// Offset=0x10 Size=0x4
};

struct _PORT_VIEW// Size=0x18 (Id=291)
{
    unsigned long Length;// Offset=0x0 Size=0x4
    void * SectionHandle;// Offset=0x4 Size=0x4
    unsigned long SectionOffset;// Offset=0x8 Size=0x4
    unsigned long ViewSize;// Offset=0xc Size=0x4
    void * ViewBase;// Offset=0x10 Size=0x4
    void * ViewRemoteBase;// Offset=0x14 Size=0x4
};

struct _CM_EISA_SLOT_INFORMATION// Size=0xc (Id=296)
{
    unsigned char ReturnCode;// Offset=0x0 Size=0x1
    unsigned char ReturnFlags;// Offset=0x1 Size=0x1
    unsigned char MajorRevision;// Offset=0x2 Size=0x1
    unsigned char MinorRevision;// Offset=0x3 Size=0x1
    unsigned short Checksum;// Offset=0x4 Size=0x2
    unsigned char NumberFunctions;// Offset=0x6 Size=0x1
    unsigned char FunctionInformation;// Offset=0x7 Size=0x1
    unsigned long CompressedId;// Offset=0x8 Size=0x4
};

struct _RTL_BITMAP_RUN// Size=0x8 (Id=297)
{
    unsigned long StartingIndex;// Offset=0x0 Size=0x4
    unsigned long NumberOfBits;// Offset=0x4 Size=0x4
};

struct _KEY_BASIC_INFORMATION// Size=0x18 (Id=299)
{
    union _LARGE_INTEGER LastWriteTime;// Offset=0x0 Size=0x8
    unsigned long TitleIndex;// Offset=0x8 Size=0x4
    unsigned long NameLength;// Offset=0xc Size=0x4
    unsigned short Name[1];// Offset=0x10 Size=0x2
};

struct _KAPC_STATE// Size=0x18 (Id=304)
{
    struct _LIST_ENTRY ApcListHead[2];// Offset=0x0 Size=0x10
    struct _KPROCESS * Process;// Offset=0x10 Size=0x4
    unsigned char KernelApcInProgress;// Offset=0x14 Size=0x1
    unsigned char KernelApcPending;// Offset=0x15 Size=0x1
    unsigned char UserApcPending;// Offset=0x16 Size=0x1
    unsigned char ApcQueueable;// Offset=0x17 Size=0x1
};

struct _SYSTEM_POOLTAG// Size=0x1c (Id=309)
{
    union // Size=0x4 (Id=0)
    {
        unsigned char Tag[4];// Offset=0x0 Size=0x4
        unsigned long TagUlong;// Offset=0x0 Size=0x4
    };
    unsigned long PagedAllocs;// Offset=0x4 Size=0x4
    unsigned long PagedFrees;// Offset=0x8 Size=0x4
    unsigned long PagedUsed;// Offset=0xc Size=0x4
    unsigned long NonPagedAllocs;// Offset=0x10 Size=0x4
    unsigned long NonPagedFrees;// Offset=0x14 Size=0x4
    unsigned long NonPagedUsed;// Offset=0x18 Size=0x4
};

struct _SYSTEM_PROCESSOR_INFORMATION// Size=0xc (Id=310)
{
    unsigned short ProcessorArchitecture;// Offset=0x0 Size=0x2
    unsigned short ProcessorLevel;// Offset=0x2 Size=0x2
    unsigned short ProcessorRevision;// Offset=0x4 Size=0x2
    unsigned short Reserved;// Offset=0x6 Size=0x2
    unsigned long ProcessorFeatureBits;// Offset=0x8 Size=0x4
};

struct _TOKEN_SOURCE// Size=0x10 (Id=317)
{
    char SourceName[8];// Offset=0x0 Size=0x8
    struct _LUID SourceIdentifier;// Offset=0x8 Size=0x8
};

struct _KERNEL_USER_TIMES// Size=0x20 (Id=318)
{
    union _LARGE_INTEGER CreateTime;// Offset=0x0 Size=0x8
    union _LARGE_INTEGER ExitTime;// Offset=0x8 Size=0x8
    union _LARGE_INTEGER KernelTime;// Offset=0x10 Size=0x8
    union _LARGE_INTEGER UserTime;// Offset=0x18 Size=0x8
};

struct _KD_SYMBOLS_INFO// Size=0x10 (Id=319)
{
    void * BaseOfDll;// Offset=0x0 Size=0x4
    unsigned long ProcessId;// Offset=0x4 Size=0x4
    unsigned long CheckSum;// Offset=0x8 Size=0x4
    unsigned long SizeOfImage;// Offset=0xc Size=0x4
};

struct _SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION// Size=0x30 (Id=323)
{
    union _LARGE_INTEGER IdleTime;// Offset=0x0 Size=0x8
    union _LARGE_INTEGER KernelTime;// Offset=0x8 Size=0x8
    union _LARGE_INTEGER UserTime;// Offset=0x10 Size=0x8
    union _LARGE_INTEGER DpcTime;// Offset=0x18 Size=0x8
    union _LARGE_INTEGER InterruptTime;// Offset=0x20 Size=0x8
    unsigned long InterruptCount;// Offset=0x28 Size=0x4
};

struct _KPROCESSOR_STATE// Size=0x28c (Id=324)
{
    struct _CONTEXT ContextFrame;// Offset=0x0 Size=0x238
    struct _KSPECIAL_REGISTERS SpecialRegisters;// Offset=0x238 Size=0x54
};

struct _FILE_COMPLETION_INFORMATION// Size=0x8 (Id=327)
{
    void * Port;// Offset=0x0 Size=0x4
    void * Key;// Offset=0x4 Size=0x4
};

struct _FILE_ACCESS_INFORMATION// Size=0x4 (Id=334)
{
    unsigned long AccessFlags;// Offset=0x0 Size=0x4
};

struct _SECURITY_TOKEN_PROXY_DATA// Size=0x18 (Id=341)
{
    unsigned long Length;// Offset=0x0 Size=0x4
    enum _PROXY_CLASS ProxyClass;// Offset=0x4 Size=0x4
    struct _UNICODE_STRING PathInfo;// Offset=0x8 Size=0x8
    unsigned long ContainerMask;// Offset=0x10 Size=0x4
    unsigned long ObjectMask;// Offset=0x14 Size=0x4
};

struct _SYSTEM_ALARM_OBJECT_ACE// Size=0x30 (Id=344)
{
    struct _ACE_HEADER Header;// Offset=0x0 Size=0x4
    unsigned long Mask;// Offset=0x4 Size=0x4
    unsigned long Flags;// Offset=0x8 Size=0x4
    struct _GUID ObjectType;// Offset=0xc Size=0x10
    struct _GUID InheritedObjectType;// Offset=0x1c Size=0x10
    unsigned long SidStart;// Offset=0x2c Size=0x4
};

struct _CM_RESOURCE_LIST// Size=0x24 (Id=352)
{
    unsigned long Count;// Offset=0x0 Size=0x4
    struct _CM_FULL_RESOURCE_DESCRIPTOR List[1];// Offset=0x4 Size=0x20
};

struct _KFLOATING_SAVE// Size=0x20 (Id=354)
{
    unsigned long ControlWord;// Offset=0x0 Size=0x4
    unsigned long StatusWord;// Offset=0x4 Size=0x4
    unsigned long ErrorOffset;// Offset=0x8 Size=0x4
    unsigned long ErrorSelector;// Offset=0xc Size=0x4
    unsigned long DataOffset;// Offset=0x10 Size=0x4
    unsigned long DataSelector;// Offset=0x14 Size=0x4
    unsigned long Cr0NpxState;// Offset=0x18 Size=0x4
    unsigned long Spare1;// Offset=0x1c Size=0x4
};

struct _IMAGE_ROM_HEADERS// Size=0x4c (Id=356)
{
    struct _IMAGE_FILE_HEADER FileHeader;// Offset=0x0 Size=0x14
    struct _IMAGE_ROM_OPTIONAL_HEADER OptionalHeader;// Offset=0x14 Size=0x38
};

struct _IMAGE_DEBUG_DIRECTORY// Size=0x1c (Id=359)
{
    unsigned long Characteristics;// Offset=0x0 Size=0x4
    unsigned long TimeDateStamp;// Offset=0x4 Size=0x4
    unsigned short MajorVersion;// Offset=0x8 Size=0x2
    unsigned short MinorVersion;// Offset=0xa Size=0x2
    unsigned long Type;// Offset=0xc Size=0x4
    unsigned long SizeOfData;// Offset=0x10 Size=0x4
    unsigned long AddressOfRawData;// Offset=0x14 Size=0x4
    unsigned long PointerToRawData;// Offset=0x18 Size=0x4
};

struct _TOKEN_USER// Size=0x8 (Id=361)
{
    struct _SID_AND_ATTRIBUTES User;// Offset=0x0 Size=0x8
};

struct _IMAGE_LINENUMBER// Size=0x6 (Id=368)
{
    union __unnamed Type;// Offset=0x0 Size=0x4
    unsigned short Linenumber;// Offset=0x4 Size=0x2
};

struct _RTL_PROCESS_LOCK_INFORMATION// Size=0x24 (Id=370)
{
    void * Address;// Offset=0x0 Size=0x4
    unsigned short Type;// Offset=0x4 Size=0x2
    unsigned short CreatorBackTraceIndex;// Offset=0x6 Size=0x2
    void * OwningThread;// Offset=0x8 Size=0x4
    long LockCount;// Offset=0xc Size=0x4
    unsigned long ContentionCount;// Offset=0x10 Size=0x4
    unsigned long EntryCount;// Offset=0x14 Size=0x4
    long RecursionCount;// Offset=0x18 Size=0x4
    unsigned long NumberOfWaitingShared;// Offset=0x1c Size=0x4
    unsigned long NumberOfWaitingExclusive;// Offset=0x20 Size=0x4
};

struct _IMAGE_ALPHA_RUNTIME_FUNCTION_ENTRY// Size=0x14 (Id=379)
{
    unsigned long BeginAddress;// Offset=0x0 Size=0x4
    unsigned long EndAddress;// Offset=0x4 Size=0x4
    unsigned long ExceptionHandler;// Offset=0x8 Size=0x4
    unsigned long HandlerData;// Offset=0xc Size=0x4
    unsigned long PrologEndAddress;// Offset=0x10 Size=0x4
};

struct _SYSTEM_POOL_INFORMATION// Size=0x1c (Id=387)
{
    unsigned long TotalSize;// Offset=0x0 Size=0x4
    void * FirstEntry;// Offset=0x4 Size=0x4
    unsigned short EntryOverhead;// Offset=0x8 Size=0x2
    unsigned char PoolTagPresent;// Offset=0xa Size=0x1
    unsigned char Spare0;// Offset=0xb Size=0x1
    unsigned long NumberOfEntries;// Offset=0xc Size=0x4
    struct _SYSTEM_POOL_ENTRY Entries[1];// Offset=0x10 Size=0xc
};

struct _SYSTEM_PROCESS_INFORMATION// Size=0xb8 (Id=389)
{
    unsigned long NextEntryOffset;// Offset=0x0 Size=0x4
    unsigned long NumberOfThreads;// Offset=0x4 Size=0x4
    union _LARGE_INTEGER SpareLi1;// Offset=0x8 Size=0x8
    union _LARGE_INTEGER SpareLi2;// Offset=0x10 Size=0x8
    union _LARGE_INTEGER SpareLi3;// Offset=0x18 Size=0x8
    union _LARGE_INTEGER CreateTime;// Offset=0x20 Size=0x8
    union _LARGE_INTEGER UserTime;// Offset=0x28 Size=0x8
    union _LARGE_INTEGER KernelTime;// Offset=0x30 Size=0x8
    struct _STRING ImageName;// Offset=0x38 Size=0x8
    long BasePriority;// Offset=0x40 Size=0x4
    void * UniqueProcessId;// Offset=0x44 Size=0x4
    void * InheritedFromUniqueProcessId;// Offset=0x48 Size=0x4
    unsigned long HandleCount;// Offset=0x4c Size=0x4
    unsigned long SessionId;// Offset=0x50 Size=0x4
    unsigned long SpareUl3;// Offset=0x54 Size=0x4
    unsigned long PeakVirtualSize;// Offset=0x58 Size=0x4
    unsigned long VirtualSize;// Offset=0x5c Size=0x4
    unsigned long PageFaultCount;// Offset=0x60 Size=0x4
    unsigned long PeakWorkingSetSize;// Offset=0x64 Size=0x4
    unsigned long WorkingSetSize;// Offset=0x68 Size=0x4
    unsigned long QuotaPeakPagedPoolUsage;// Offset=0x6c Size=0x4
    unsigned long QuotaPagedPoolUsage;// Offset=0x70 Size=0x4
    unsigned long QuotaPeakNonPagedPoolUsage;// Offset=0x74 Size=0x4
    unsigned long QuotaNonPagedPoolUsage;// Offset=0x78 Size=0x4
    unsigned long PagefileUsage;// Offset=0x7c Size=0x4
    unsigned long PeakPagefileUsage;// Offset=0x80 Size=0x4
    unsigned long PrivatePageCount;// Offset=0x84 Size=0x4
    union _LARGE_INTEGER ReadOperationCount;// Offset=0x88 Size=0x8
    union _LARGE_INTEGER WriteOperationCount;// Offset=0x90 Size=0x8
    union _LARGE_INTEGER OtherOperationCount;// Offset=0x98 Size=0x8
    union _LARGE_INTEGER ReadTransferCount;// Offset=0xa0 Size=0x8
    union _LARGE_INTEGER WriteTransferCount;// Offset=0xa8 Size=0x8
    union _LARGE_INTEGER OtherTransferCount;// Offset=0xb0 Size=0x8
};

struct _KEY_VALUE_ENTRY// Size=0x10 (Id=390)
{
    struct _UNICODE_STRING * ValueName;// Offset=0x0 Size=0x4
    unsigned long DataLength;// Offset=0x4 Size=0x4
    unsigned long DataOffset;// Offset=0x8 Size=0x4
    unsigned long Type;// Offset=0xc Size=0x4
};

struct _RTL_HEAP_USAGE_ENTRY// Size=0x10 (Id=394)
{
    struct _RTL_HEAP_USAGE_ENTRY * Next;// Offset=0x0 Size=0x4
    void * Address;// Offset=0x4 Size=0x4
    unsigned long Size;// Offset=0x8 Size=0x4
    unsigned short AllocatorBackTraceIndex;// Offset=0xc Size=0x2
    unsigned short TagIndex;// Offset=0xe Size=0x2
};

struct _FILE_NETWORK_OPEN_INFORMATION// Size=0x38 (Id=398)
{
    union _LARGE_INTEGER CreationTime;// Offset=0x0 Size=0x8
    union _LARGE_INTEGER LastAccessTime;// Offset=0x8 Size=0x8
    union _LARGE_INTEGER LastWriteTime;// Offset=0x10 Size=0x8
    union _LARGE_INTEGER ChangeTime;// Offset=0x18 Size=0x8
    union _LARGE_INTEGER AllocationSize;// Offset=0x20 Size=0x8
    union _LARGE_INTEGER EndOfFile;// Offset=0x28 Size=0x8
    unsigned long FileAttributes;// Offset=0x30 Size=0x4
};

struct _KTHREAD// Size=0x110 (Id=404)
{
    struct _DISPATCHER_HEADER Header;// Offset=0x0 Size=0x10
    struct _LIST_ENTRY MutantListHead;// Offset=0x10 Size=0x8
    unsigned long KernelTime;// Offset=0x18 Size=0x4
    void * StackBase;// Offset=0x1c Size=0x4
    void * StackLimit;// Offset=0x20 Size=0x4
    void * KernelStack;// Offset=0x24 Size=0x4
    void * TlsData;// Offset=0x28 Size=0x4
    unsigned char State;// Offset=0x2c Size=0x1
    unsigned char Alerted[2];// Offset=0x2d Size=0x2
    unsigned char Alertable;// Offset=0x2f Size=0x1
    unsigned char NpxState;// Offset=0x30 Size=0x1
    char Saturation;// Offset=0x31 Size=0x1
    char Priority;// Offset=0x32 Size=0x1
    unsigned char Padding;// Offset=0x33 Size=0x1
    struct _KAPC_STATE ApcState;// Offset=0x34 Size=0x18
    unsigned long ContextSwitches;// Offset=0x4c Size=0x4
    long WaitStatus;// Offset=0x50 Size=0x4
    unsigned char WaitIrql;// Offset=0x54 Size=0x1
    char WaitMode;// Offset=0x55 Size=0x1
    unsigned char WaitNext;// Offset=0x56 Size=0x1
    unsigned char WaitReason;// Offset=0x57 Size=0x1
    struct _KWAIT_BLOCK * WaitBlockList;// Offset=0x58 Size=0x4
    struct _LIST_ENTRY WaitListEntry;// Offset=0x5c Size=0x8
    unsigned long WaitTime;// Offset=0x64 Size=0x4
    unsigned long KernelApcDisable;// Offset=0x68 Size=0x4
    long Quantum;// Offset=0x6c Size=0x4
    char BasePriority;// Offset=0x70 Size=0x1
    unsigned char DecrementCount;// Offset=0x71 Size=0x1
    char PriorityDecrement;// Offset=0x72 Size=0x1
    unsigned char DisableBoost;// Offset=0x73 Size=0x1
    unsigned char NpxIrql;// Offset=0x74 Size=0x1
    char SuspendCount;// Offset=0x75 Size=0x1
    unsigned char Preempted;// Offset=0x76 Size=0x1
    unsigned char HasTerminated;// Offset=0x77 Size=0x1
    struct _KQUEUE * Queue;// Offset=0x78 Size=0x4
    struct _LIST_ENTRY QueueListEntry;// Offset=0x7c Size=0x8
    unsigned char __align0[4];// Offset=0x84 Size=0x4
    struct _KTIMER Timer;// Offset=0x88 Size=0x28
    struct _KWAIT_BLOCK TimerWaitBlock;// Offset=0xb0 Size=0x18
    struct _KAPC SuspendApc;// Offset=0xc8 Size=0x28
    struct _KSEMAPHORE SuspendSemaphore;// Offset=0xf0 Size=0x14
    struct _LIST_ENTRY ThreadListEntry;// Offset=0x104 Size=0x8
};

struct _IMAGE_CE_RUNTIME_FUNCTION_ENTRY// Size=0x8 (Id=409)
{
    unsigned long FuncStart;// Offset=0x0 Size=0x4
    struct // Size=0x4 (Id=0)
    {
        unsigned long PrologLen:8;// Offset=0x4 Size=0x4 BitOffset=0x0 BitSize=0x8
        unsigned long FuncLen:22;// Offset=0x4 Size=0x4 BitOffset=0x8 BitSize=0x16
        unsigned long ThirtyTwoBit:1;// Offset=0x4 Size=0x4 BitOffset=0x1e BitSize=0x1
        unsigned long ExceptionFlag:1;// Offset=0x4 Size=0x4 BitOffset=0x1f BitSize=0x1
    };
};

struct _SID_IDENTIFIER_AUTHORITY// Size=0x6 (Id=414)
{
    unsigned char Value[6];// Offset=0x0 Size=0x6
};

struct _FILE_FS_FULL_SIZE_INFORMATION// Size=0x20 (Id=415)
{
    union _LARGE_INTEGER TotalAllocationUnits;// Offset=0x0 Size=0x8
    union _LARGE_INTEGER CallerAvailableAllocationUnits;// Offset=0x8 Size=0x8
    union _LARGE_INTEGER ActualAvailableAllocationUnits;// Offset=0x10 Size=0x8
    unsigned long SectorsPerAllocationUnit;// Offset=0x18 Size=0x4
    unsigned long BytesPerSector;// Offset=0x1c Size=0x4
};

struct _IMAGE_VXD_HEADER// Size=0xc4 (Id=417)
{
    unsigned short e32_magic;// Offset=0x0 Size=0x2
    unsigned char e32_border;// Offset=0x2 Size=0x1
    unsigned char e32_worder;// Offset=0x3 Size=0x1
    unsigned long e32_level;// Offset=0x4 Size=0x4
    unsigned short e32_cpu;// Offset=0x8 Size=0x2
    unsigned short e32_os;// Offset=0xa Size=0x2
    unsigned long e32_ver;// Offset=0xc Size=0x4
    unsigned long e32_mflags;// Offset=0x10 Size=0x4
    unsigned long e32_mpages;// Offset=0x14 Size=0x4
    unsigned long e32_startobj;// Offset=0x18 Size=0x4
    unsigned long e32_eip;// Offset=0x1c Size=0x4
    unsigned long e32_stackobj;// Offset=0x20 Size=0x4
    unsigned long e32_esp;// Offset=0x24 Size=0x4
    unsigned long e32_pagesize;// Offset=0x28 Size=0x4
    unsigned long e32_lastpagesize;// Offset=0x2c Size=0x4
    unsigned long e32_fixupsize;// Offset=0x30 Size=0x4
    unsigned long e32_fixupsum;// Offset=0x34 Size=0x4
    unsigned long e32_ldrsize;// Offset=0x38 Size=0x4
    unsigned long e32_ldrsum;// Offset=0x3c Size=0x4
    unsigned long e32_objtab;// Offset=0x40 Size=0x4
    unsigned long e32_objcnt;// Offset=0x44 Size=0x4
    unsigned long e32_objmap;// Offset=0x48 Size=0x4
    unsigned long e32_itermap;// Offset=0x4c Size=0x4
    unsigned long e32_rsrctab;// Offset=0x50 Size=0x4
    unsigned long e32_rsrccnt;// Offset=0x54 Size=0x4
    unsigned long e32_restab;// Offset=0x58 Size=0x4
    unsigned long e32_enttab;// Offset=0x5c Size=0x4
    unsigned long e32_dirtab;// Offset=0x60 Size=0x4
    unsigned long e32_dircnt;// Offset=0x64 Size=0x4
    unsigned long e32_fpagetab;// Offset=0x68 Size=0x4
    unsigned long e32_frectab;// Offset=0x6c Size=0x4
    unsigned long e32_impmod;// Offset=0x70 Size=0x4
    unsigned long e32_impmodcnt;// Offset=0x74 Size=0x4
    unsigned long e32_impproc;// Offset=0x78 Size=0x4
    unsigned long e32_pagesum;// Offset=0x7c Size=0x4
    unsigned long e32_datapage;// Offset=0x80 Size=0x4
    unsigned long e32_preload;// Offset=0x84 Size=0x4
    unsigned long e32_nrestab;// Offset=0x88 Size=0x4
    unsigned long e32_cbnrestab;// Offset=0x8c Size=0x4
    unsigned long e32_nressum;// Offset=0x90 Size=0x4
    unsigned long e32_autodata;// Offset=0x94 Size=0x4
    unsigned long e32_debuginfo;// Offset=0x98 Size=0x4
    unsigned long e32_debuglen;// Offset=0x9c Size=0x4
    unsigned long e32_instpreload;// Offset=0xa0 Size=0x4
    unsigned long e32_instdemand;// Offset=0xa4 Size=0x4
    unsigned long e32_heapsize;// Offset=0xa8 Size=0x4
    unsigned char e32_res3[12];// Offset=0xac Size=0xc
    unsigned long e32_winresoff;// Offset=0xb8 Size=0x4
    unsigned long e32_winreslen;// Offset=0xbc Size=0x4
    unsigned short e32_devid;// Offset=0xc0 Size=0x2
    unsigned short e32_ddkver;// Offset=0xc2 Size=0x2
};

struct _CM_MCA_POS_DATA// Size=0x6 (Id=420)
{
    unsigned short AdapterId;// Offset=0x0 Size=0x2
    unsigned char PosData1;// Offset=0x2 Size=0x1
    unsigned char PosData2;// Offset=0x3 Size=0x1
    unsigned char PosData3;// Offset=0x4 Size=0x1
    unsigned char PosData4;// Offset=0x5 Size=0x1
};

struct _FILE_MAILSLOT_SET_INFORMATION// Size=0x4 (Id=423)
{
    union _LARGE_INTEGER * ReadTimeout;// Offset=0x0 Size=0x4
};

struct _RTL_RELATIVE_NAME// Size=0xc (Id=424)
{
    struct _STRING RelativeName;// Offset=0x0 Size=0x8
    void * ContainingDirectory;// Offset=0x8 Size=0x4
};

struct _IMAGE_OPTIONAL_HEADER// Size=0xe0 (Id=426)
{
    unsigned short Magic;// Offset=0x0 Size=0x2
    unsigned char MajorLinkerVersion;// Offset=0x2 Size=0x1
    unsigned char MinorLinkerVersion;// Offset=0x3 Size=0x1
    unsigned long SizeOfCode;// Offset=0x4 Size=0x4
    unsigned long SizeOfInitializedData;// Offset=0x8 Size=0x4
    unsigned long SizeOfUninitializedData;// Offset=0xc Size=0x4
    unsigned long AddressOfEntryPoint;// Offset=0x10 Size=0x4
    unsigned long BaseOfCode;// Offset=0x14 Size=0x4
    unsigned long BaseOfData;// Offset=0x18 Size=0x4
    unsigned long ImageBase;// Offset=0x1c Size=0x4
    unsigned long SectionAlignment;// Offset=0x20 Size=0x4
    unsigned long FileAlignment;// Offset=0x24 Size=0x4
    unsigned short MajorOperatingSystemVersion;// Offset=0x28 Size=0x2
    unsigned short MinorOperatingSystemVersion;// Offset=0x2a Size=0x2
    unsigned short MajorImageVersion;// Offset=0x2c Size=0x2
    unsigned short MinorImageVersion;// Offset=0x2e Size=0x2
    unsigned short MajorSubsystemVersion;// Offset=0x30 Size=0x2
    unsigned short MinorSubsystemVersion;// Offset=0x32 Size=0x2
    unsigned long Win32VersionValue;// Offset=0x34 Size=0x4
    unsigned long SizeOfImage;// Offset=0x38 Size=0x4
    unsigned long SizeOfHeaders;// Offset=0x3c Size=0x4
    unsigned long CheckSum;// Offset=0x40 Size=0x4
    unsigned short Subsystem;// Offset=0x44 Size=0x2
    unsigned short DllCharacteristics;// Offset=0x46 Size=0x2
    unsigned long SizeOfStackReserve;// Offset=0x48 Size=0x4
    unsigned long SizeOfStackCommit;// Offset=0x4c Size=0x4
    unsigned long SizeOfHeapReserve;// Offset=0x50 Size=0x4
    unsigned long SizeOfHeapCommit;// Offset=0x54 Size=0x4
    unsigned long LoaderFlags;// Offset=0x58 Size=0x4
    unsigned long NumberOfRvaAndSizes;// Offset=0x5c Size=0x4
    struct _IMAGE_DATA_DIRECTORY DataDirectory[16];// Offset=0x60 Size=0x80
};

struct _IMAGE_COFF_SYMBOLS_HEADER// Size=0x20 (Id=427)
{
    unsigned long NumberOfSymbols;// Offset=0x0 Size=0x4
    unsigned long LvaToFirstSymbol;// Offset=0x4 Size=0x4
    unsigned long NumberOfLinenumbers;// Offset=0x8 Size=0x4
    unsigned long LvaToFirstLinenumber;// Offset=0xc Size=0x4
    unsigned long RvaToFirstByteOfCode;// Offset=0x10 Size=0x4
    unsigned long RvaToLastByteOfCode;// Offset=0x14 Size=0x4
    unsigned long RvaToFirstByteOfData;// Offset=0x18 Size=0x4
    unsigned long RvaToLastByteOfData;// Offset=0x1c Size=0x4
};

struct _IMAGE_SYMBOL// Size=0x12 (Id=432)
{
    union __unnamed N;// Offset=0x0 Size=0x8
    unsigned long Value;// Offset=0x8 Size=0x4
    short SectionNumber;// Offset=0xc Size=0x2
    unsigned short Type;// Offset=0xe Size=0x2
    unsigned char StorageClass;// Offset=0x10 Size=0x1
    unsigned char NumberOfAuxSymbols;// Offset=0x11 Size=0x1
};

struct _SYSTEM_CRASH_STATE_INFORMATION// Size=0x8 (Id=443)
{
    unsigned long ValidCrashDump;// Offset=0x0 Size=0x4
    unsigned long ValidDirectDump;// Offset=0x4 Size=0x4
};

struct _FILE_FS_LABEL_INFORMATION// Size=0x8 (Id=448)
{
    unsigned long VolumeLabelLength;// Offset=0x0 Size=0x4
    char VolumeLabel[1];// Offset=0x4 Size=0x1
};

struct _IO_RESOURCE_LIST// Size=0x28 (Id=450)
{
    unsigned short Version;// Offset=0x0 Size=0x2
    unsigned short Revision;// Offset=0x2 Size=0x2
    unsigned long Count;// Offset=0x4 Size=0x4
    struct _IO_RESOURCE_DESCRIPTOR Descriptors[1];// Offset=0x8 Size=0x20
};

struct _IMAGE_THUNK_DATA32// Size=0x4 (Id=452)
{
    union __unnamed u1;// Offset=0x0 Size=0x4
};

struct _RTL_PROCESS_BACKTRACE_INFORMATION// Size=0x8c (Id=454)
{
    char * SymbolicBackTrace;// Offset=0x0 Size=0x4
    unsigned long TraceCount;// Offset=0x4 Size=0x4
    unsigned short Index;// Offset=0x8 Size=0x2
    unsigned short Depth;// Offset=0xa Size=0x2
    void * BackTrace[32];// Offset=0xc Size=0x80
};

struct _SYSTEM_VERIFIER_INFORMATION// Size=0x68 (Id=456)
{
    unsigned long NextEntryOffset;// Offset=0x0 Size=0x4
    unsigned long Level;// Offset=0x4 Size=0x4
    struct _STRING DriverName;// Offset=0x8 Size=0x8
    unsigned long RaiseIrqls;// Offset=0x10 Size=0x4
    unsigned long AcquireSpinLocks;// Offset=0x14 Size=0x4
    unsigned long SynchronizeExecutions;// Offset=0x18 Size=0x4
    unsigned long AllocationsAttempted;// Offset=0x1c Size=0x4
    unsigned long AllocationsSucceeded;// Offset=0x20 Size=0x4
    unsigned long AllocationsSucceededSpecialPool;// Offset=0x24 Size=0x4
    unsigned long AllocationsWithNoTag;// Offset=0x28 Size=0x4
    unsigned long TrimRequests;// Offset=0x2c Size=0x4
    unsigned long Trims;// Offset=0x30 Size=0x4
    unsigned long AllocationsFailed;// Offset=0x34 Size=0x4
    unsigned long AllocationsFailedDeliberately;// Offset=0x38 Size=0x4
    unsigned long Loads;// Offset=0x3c Size=0x4
    unsigned long Unloads;// Offset=0x40 Size=0x4
    unsigned long UnTrackedPool;// Offset=0x44 Size=0x4
    unsigned long CurrentPagedPoolAllocations;// Offset=0x48 Size=0x4
    unsigned long CurrentNonPagedPoolAllocations;// Offset=0x4c Size=0x4
    unsigned long PeakPagedPoolAllocations;// Offset=0x50 Size=0x4
    unsigned long PeakNonPagedPoolAllocations;// Offset=0x54 Size=0x4
    unsigned long PagedPoolUsageInBytes;// Offset=0x58 Size=0x4
    unsigned long NonPagedPoolUsageInBytes;// Offset=0x5c Size=0x4
    unsigned long PeakPagedPoolUsageInBytes;// Offset=0x60 Size=0x4
    unsigned long PeakNonPagedPoolUsageInBytes;// Offset=0x64 Size=0x4
};

struct _CM_SERIAL_DEVICE_DATA// Size=0x8 (Id=457)
{
    unsigned short Version;// Offset=0x0 Size=0x2
    unsigned short Revision;// Offset=0x2 Size=0x2
    unsigned long BaudClock;// Offset=0x4 Size=0x4
};

struct _KEY_VALUE_FULL_INFORMATION// Size=0x18 (Id=460)
{
    unsigned long TitleIndex;// Offset=0x0 Size=0x4
    unsigned long Type;// Offset=0x4 Size=0x4
    unsigned long DataOffset;// Offset=0x8 Size=0x4
    unsigned long DataLength;// Offset=0xc Size=0x4
    unsigned long NameLength;// Offset=0x10 Size=0x4
    unsigned short Name[1];// Offset=0x14 Size=0x2
};

struct _SYSTEM_SESSION_PROCESS_INFORMATION// Size=0xc (Id=463)
{
    unsigned long SessionId;// Offset=0x0 Size=0x4
    unsigned long SizeOfBuf;// Offset=0x4 Size=0x4
    void * Buffer;// Offset=0x8 Size=0x4
};

struct _SYSTEM_CALL_TIME_INFORMATION// Size=0x10 (Id=476)
{
    unsigned long Length;// Offset=0x0 Size=0x4
    unsigned long TotalCalls;// Offset=0x4 Size=0x4
    union _LARGE_INTEGER TimeOfCalls[1];// Offset=0x8 Size=0x8
};

struct _KTSS// Size=0x20ac (Id=478)
{
    unsigned short Backlink;// Offset=0x0 Size=0x2
    unsigned short Reserved0;// Offset=0x2 Size=0x2
    unsigned long Esp0;// Offset=0x4 Size=0x4
    unsigned short Ss0;// Offset=0x8 Size=0x2
    unsigned short Reserved1;// Offset=0xa Size=0x2
    unsigned long Esp1;// Offset=0xc Size=0x4
    unsigned short Ss1;// Offset=0x10 Size=0x2
    unsigned short Reserved2;// Offset=0x12 Size=0x2
    unsigned long Esp2;// Offset=0x14 Size=0x4
    unsigned short Ss2;// Offset=0x18 Size=0x2
    unsigned short Reserved3;// Offset=0x1a Size=0x2
    unsigned long CR3;// Offset=0x1c Size=0x4
    unsigned long Eip;// Offset=0x20 Size=0x4
    unsigned long EFlags;// Offset=0x24 Size=0x4
    unsigned long Eax;// Offset=0x28 Size=0x4
    unsigned long Ecx;// Offset=0x2c Size=0x4
    unsigned long Edx;// Offset=0x30 Size=0x4
    unsigned long Ebx;// Offset=0x34 Size=0x4
    unsigned long Esp;// Offset=0x38 Size=0x4
    unsigned long Ebp;// Offset=0x3c Size=0x4
    unsigned long Esi;// Offset=0x40 Size=0x4
    unsigned long Edi;// Offset=0x44 Size=0x4
    unsigned short Es;// Offset=0x48 Size=0x2
    unsigned short Reserved4;// Offset=0x4a Size=0x2
    unsigned short Cs;// Offset=0x4c Size=0x2
    unsigned short Reserved5;// Offset=0x4e Size=0x2
    unsigned short Ss;// Offset=0x50 Size=0x2
    unsigned short Reserved6;// Offset=0x52 Size=0x2
    unsigned short Ds;// Offset=0x54 Size=0x2
    unsigned short Reserved7;// Offset=0x56 Size=0x2
    unsigned short Fs;// Offset=0x58 Size=0x2
    unsigned short Reserved8;// Offset=0x5a Size=0x2
    unsigned short Gs;// Offset=0x5c Size=0x2
    unsigned short Reserved9;// Offset=0x5e Size=0x2
    unsigned short LDT;// Offset=0x60 Size=0x2
    unsigned short Reserved10;// Offset=0x62 Size=0x2
    unsigned short Flags;// Offset=0x64 Size=0x2
    unsigned short IoMapBase;// Offset=0x66 Size=0x2
    struct _KiIoAccessMap IoMaps[1];// Offset=0x68 Size=0x2024
    unsigned char IntDirectionMap[32];// Offset=0x208c Size=0x20
};

struct _KPRCB// Size=0x25c (Id=484)
{
    struct _KTHREAD * CurrentThread;// Offset=0x0 Size=0x4
    struct _KTHREAD * NextThread;// Offset=0x4 Size=0x4
    struct _KTHREAD * IdleThread;// Offset=0x8 Size=0x4
    struct _KTHREAD * NpxThread;// Offset=0xc Size=0x4
    unsigned long InterruptCount;// Offset=0x10 Size=0x4
    unsigned long DpcTime;// Offset=0x14 Size=0x4
    unsigned long InterruptTime;// Offset=0x18 Size=0x4
    unsigned long DebugDpcTime;// Offset=0x1c Size=0x4
    unsigned long KeContextSwitches;// Offset=0x20 Size=0x4
    unsigned long DpcInterruptRequested;// Offset=0x24 Size=0x4
    struct _LIST_ENTRY DpcListHead;// Offset=0x28 Size=0x8
    unsigned long DpcRoutineActive;// Offset=0x30 Size=0x4
    void * DpcStack;// Offset=0x34 Size=0x4
    unsigned long QuantumEnd;// Offset=0x38 Size=0x4
    struct _FX_SAVE_AREA NpxSaveArea;// Offset=0x3c Size=0x210
    void * DmEnetFunc;// Offset=0x24c Size=0x4
    void * DebugMonitorData;// Offset=0x250 Size=0x4
    void * DebugHaltThread;// Offset=0x254 Size=0x4
    void * DebugDoubleFault;// Offset=0x258 Size=0x4
};

struct IMPORT_OBJECT_HEADER// Size=0x14 (Id=486)
{
    unsigned short Sig1;// Offset=0x0 Size=0x2
    unsigned short Sig2;// Offset=0x2 Size=0x2
    unsigned short Version;// Offset=0x4 Size=0x2
    unsigned short Machine;// Offset=0x6 Size=0x2
    unsigned long TimeDateStamp;// Offset=0x8 Size=0x4
    unsigned long SizeOfData;// Offset=0xc Size=0x4
    union // Size=0x2 (Id=0)
    {
        unsigned short Ordinal;// Offset=0x10 Size=0x2
        unsigned short Hint;// Offset=0x10 Size=0x2
    };
    struct // Size=0x2 (Id=0)
    {
        unsigned short Type:2;// Offset=0x12 Size=0x2 BitOffset=0x0 BitSize=0x2
        unsigned short NameType:3;// Offset=0x12 Size=0x2 BitOffset=0x2 BitSize=0x3
        unsigned short Reserved:11;// Offset=0x12 Size=0x2 BitOffset=0x5 BitSize=0xb
    };
};

struct _SYSTEM_HANDLE_INFORMATION// Size=0x14 (Id=489)
{
    unsigned long NumberOfHandles;// Offset=0x0 Size=0x4
    struct _SYSTEM_HANDLE_TABLE_ENTRY_INFO Handles[1];// Offset=0x4 Size=0x10
};

struct _HARDERROR_MSG// Size=0x50 (Id=492)
{
    struct _PORT_MESSAGE h;// Offset=0x0 Size=0x18
    long Status;// Offset=0x18 Size=0x4
    unsigned char __align0[4];// Offset=0x1c Size=0x4
    union _LARGE_INTEGER ErrorTime;// Offset=0x20 Size=0x8
    unsigned long ValidResponseOptions;// Offset=0x28 Size=0x4
    unsigned long Response;// Offset=0x2c Size=0x4
    unsigned long NumberOfParameters;// Offset=0x30 Size=0x4
    unsigned long UnicodeStringParameterMask;// Offset=0x34 Size=0x4
    unsigned long Parameters[5];// Offset=0x38 Size=0x14
};

struct _KQUEUE// Size=0x28 (Id=493)
{
    struct _DISPATCHER_HEADER Header;// Offset=0x0 Size=0x10
    struct _LIST_ENTRY EntryListHead;// Offset=0x10 Size=0x8
    unsigned long CurrentCount;// Offset=0x18 Size=0x4
    unsigned long MaximumCount;// Offset=0x1c Size=0x4
    struct _LIST_ENTRY ThreadListHead;// Offset=0x20 Size=0x8
};

struct _IMAGE_FILE_HEADER// Size=0x14 (Id=496)
{
    unsigned short Machine;// Offset=0x0 Size=0x2
    unsigned short NumberOfSections;// Offset=0x2 Size=0x2
    unsigned long TimeDateStamp;// Offset=0x4 Size=0x4
    unsigned long PointerToSymbolTable;// Offset=0x8 Size=0x4
    unsigned long NumberOfSymbols;// Offset=0xc Size=0x4
    unsigned short SizeOfOptionalHeader;// Offset=0x10 Size=0x2
    unsigned short Characteristics;// Offset=0x12 Size=0x2
};

struct _KEY_VALUE_PARTIAL_INFORMATION// Size=0x10 (Id=497)
{
    unsigned long TitleIndex;// Offset=0x0 Size=0x4
    unsigned long Type;// Offset=0x4 Size=0x4
    unsigned long DataLength;// Offset=0x8 Size=0x4
    unsigned char Data[1];// Offset=0xc Size=0x1
};

struct _KEY_NAME_INFORMATION// Size=0x8 (Id=501)
{
    unsigned long NameLength;// Offset=0x0 Size=0x4
    unsigned short Name[1];// Offset=0x4 Size=0x2
};

struct _RTL_PROCESS_HEAPS// Size=0x44 (Id=502)
{
    unsigned long NumberOfHeaps;// Offset=0x0 Size=0x4
    struct _RTL_HEAP_INFORMATION Heaps[1];// Offset=0x4 Size=0x40
};

struct _IMAGE_RELOCATION// Size=0xa (Id=521)
{
    union // Size=0x4 (Id=0)
    {
        unsigned long VirtualAddress;// Offset=0x0 Size=0x4
        unsigned long RelocCount;// Offset=0x0 Size=0x4
    };
    unsigned long SymbolTableIndex;// Offset=0x4 Size=0x4
    unsigned short Type;// Offset=0x8 Size=0x2
};

struct _KSYSTEM_TIME// Size=0xc (Id=524)
{
    unsigned long LowPart;// Offset=0x0 Size=0x4
    long High1Time;// Offset=0x4 Size=0x4
    long High2Time;// Offset=0x8 Size=0x4
};

struct _FILE_MOVE_CLUSTER_INFORMATION// Size=0x10 (Id=526)
{
    unsigned long ClusterCount;// Offset=0x0 Size=0x4
    void * RootDirectory;// Offset=0x4 Size=0x4
    unsigned long FileNameLength;// Offset=0x8 Size=0x4
    char FileName[1];// Offset=0xc Size=0x1
};

struct _IMAGE_SEPARATE_DEBUG_HEADER// Size=0x30 (Id=532)
{
    unsigned short Signature;// Offset=0x0 Size=0x2
    unsigned short Flags;// Offset=0x2 Size=0x2
    unsigned short Machine;// Offset=0x4 Size=0x2
    unsigned short Characteristics;// Offset=0x6 Size=0x2
    unsigned long TimeDateStamp;// Offset=0x8 Size=0x4
    unsigned long CheckSum;// Offset=0xc Size=0x4
    unsigned long ImageBase;// Offset=0x10 Size=0x4
    unsigned long SizeOfImage;// Offset=0x14 Size=0x4
    unsigned long NumberOfSections;// Offset=0x18 Size=0x4
    unsigned long ExportedNamesSize;// Offset=0x1c Size=0x4
    unsigned long DebugDirectorySize;// Offset=0x20 Size=0x4
    unsigned long SectionAlignment;// Offset=0x24 Size=0x4
    unsigned long Reserved[2];// Offset=0x28 Size=0x8
};

struct _FILE_LINK_INFORMATION// Size=0x10 (Id=534)
{
    unsigned char ReplaceIfExists;// Offset=0x0 Size=0x1
    unsigned char __align0[3];// Offset=0x1 Size=0x3
    void * RootDirectory;// Offset=0x4 Size=0x4
    unsigned long FileNameLength;// Offset=0x8 Size=0x4
    char FileName[1];// Offset=0xc Size=0x1
};

struct _FILE_ATTRIBUTE_TAG_INFORMATION// Size=0x8 (Id=540)
{
    unsigned long FileAttributes;// Offset=0x0 Size=0x4
    unsigned long ReparseTag;// Offset=0x4 Size=0x4
};

struct _RTL_HANDLE_TABLE// Size=0x20 (Id=541)
{
    unsigned long MaximumNumberOfHandles;// Offset=0x0 Size=0x4
    unsigned long SizeOfHandleTableEntry;// Offset=0x4 Size=0x4
    unsigned long Reserved[2];// Offset=0x8 Size=0x8
    struct _RTL_HANDLE_TABLE_ENTRY * FreeHandles;// Offset=0x10 Size=0x4
    struct _RTL_HANDLE_TABLE_ENTRY * CommittedHandles;// Offset=0x14 Size=0x4
    struct _RTL_HANDLE_TABLE_ENTRY * UnCommittedHandles;// Offset=0x18 Size=0x4
    struct _RTL_HANDLE_TABLE_ENTRY * MaxReservedHandles;// Offset=0x1c Size=0x4
};

struct _FILE_ALLOCATION_INFORMATION// Size=0x8 (Id=543)
{
    union _LARGE_INTEGER AllocationSize;// Offset=0x0 Size=0x8
};

struct _RTL_DEBUG_INFORMATION// Size=0x60 (Id=544)
{
    void * SectionHandleClient;// Offset=0x0 Size=0x4
    void * ViewBaseClient;// Offset=0x4 Size=0x4
    void * ViewBaseTarget;// Offset=0x8 Size=0x4
    unsigned long ViewBaseDelta;// Offset=0xc Size=0x4
    void * EventPairClient;// Offset=0x10 Size=0x4
    void * EventPairTarget;// Offset=0x14 Size=0x4
    void * TargetProcessId;// Offset=0x18 Size=0x4
    void * TargetThreadHandle;// Offset=0x1c Size=0x4
    unsigned long Flags;// Offset=0x20 Size=0x4
    unsigned long OffsetFree;// Offset=0x24 Size=0x4
    unsigned long CommitSize;// Offset=0x28 Size=0x4
    unsigned long ViewSize;// Offset=0x2c Size=0x4
    struct _RTL_PROCESS_MODULES * Modules;// Offset=0x30 Size=0x4
    struct _RTL_PROCESS_BACKTRACES * BackTraces;// Offset=0x34 Size=0x4
    struct _RTL_PROCESS_HEAPS * Heaps;// Offset=0x38 Size=0x4
    struct _RTL_PROCESS_LOCKS * Locks;// Offset=0x3c Size=0x4
    void * SpecificHeap;// Offset=0x40 Size=0x4
    void * TargetProcessHandle;// Offset=0x44 Size=0x4
    void * Reserved[6];// Offset=0x48 Size=0x18
};

struct _CM_INT13_DRIVE_PARAMETER// Size=0xc (Id=555)
{
    unsigned short DriveSelect;// Offset=0x0 Size=0x2
    unsigned long MaxCylinders;// Offset=0x2 Size=0x4
    unsigned short SectorsPerTrack;// Offset=0x6 Size=0x2
    unsigned short MaxHeads;// Offset=0x8 Size=0x2
    unsigned short NumberDrives;// Offset=0xa Size=0x2
};

struct _IMAGE_ARCHIVE_MEMBER_HEADER// Size=0x3c (Id=559)
{
    unsigned char Name[16];// Offset=0x0 Size=0x10
    unsigned char Date[12];// Offset=0x10 Size=0xc
    unsigned char UserID[6];// Offset=0x1c Size=0x6
    unsigned char GroupID[6];// Offset=0x22 Size=0x6
    unsigned char Mode[8];// Offset=0x28 Size=0x8
    unsigned char Size[10];// Offset=0x30 Size=0xa
    unsigned char EndHeader[2];// Offset=0x3a Size=0x2
};

struct _SYSTEM_KERNEL_DEBUGGER_INFORMATION// Size=0x2 (Id=562)
{
    unsigned char KernelDebuggerEnabled;// Offset=0x0 Size=0x1
    unsigned char KernelDebuggerNotPresent;// Offset=0x1 Size=0x1
};

struct _PROCESS_PRIORITY_CLASS// Size=0x2 (Id=564)
{
    unsigned char Foreground;// Offset=0x0 Size=0x1
    unsigned char PriorityClass;// Offset=0x1 Size=0x1
};

struct _EVENT_BASIC_INFORMATION// Size=0x8 (Id=565)
{
    enum _EVENT_TYPE EventType;// Offset=0x0 Size=0x4
    long EventState;// Offset=0x4 Size=0x4
};

struct _IMAGE_EXPORT_DIRECTORY// Size=0x28 (Id=567)
{
    unsigned long Characteristics;// Offset=0x0 Size=0x4
    unsigned long TimeDateStamp;// Offset=0x4 Size=0x4
    unsigned short MajorVersion;// Offset=0x8 Size=0x2
    unsigned short MinorVersion;// Offset=0xa Size=0x2
    unsigned long Name;// Offset=0xc Size=0x4
    unsigned long Base;// Offset=0x10 Size=0x4
    unsigned long NumberOfFunctions;// Offset=0x14 Size=0x4
    unsigned long NumberOfNames;// Offset=0x18 Size=0x4
    unsigned long AddressOfFunctions;// Offset=0x1c Size=0x4
    unsigned long AddressOfNames;// Offset=0x20 Size=0x4
    unsigned long AddressOfNameOrdinals;// Offset=0x24 Size=0x4
};

struct _RTL_DRIVE_LETTER_CURDIR// Size=0x10 (Id=569)
{
    unsigned short Flags;// Offset=0x0 Size=0x2
    unsigned short Length;// Offset=0x2 Size=0x2
    unsigned long TimeStamp;// Offset=0x4 Size=0x4
    struct _STRING DosPath;// Offset=0x8 Size=0x8
};

union _POWER_STATE// Size=0x4 (Id=571)
{
    enum _SYSTEM_POWER_STATE SystemState;// Offset=0x0 Size=0x4
    enum _DEVICE_POWER_STATE DeviceState;// Offset=0x0 Size=0x4
};

union _FILE_SEGMENT_ELEMENT// Size=0x4 (Id=583)
{
    void * Buffer;// Offset=0x0 Size=0x4
    unsigned long Alignment;// Offset=0x0 Size=0x4
};

struct _FILE_END_OF_FILE_INFORMATION// Size=0x8 (Id=589)
{
    union _LARGE_INTEGER EndOfFile;// Offset=0x0 Size=0x8
};

struct _PORT_DATA_ENTRY// Size=0x8 (Id=595)
{
    void * Base;// Offset=0x0 Size=0x4
    unsigned long Size;// Offset=0x4 Size=0x4
};

struct _CM_COMPONENT_INFORMATION// Size=0x10 (Id=602)
{
    struct _DEVICE_FLAGS Flags;// Offset=0x0 Size=0x4
    unsigned long Version;// Offset=0x4 Size=0x4
    unsigned long Key;// Offset=0x8 Size=0x4
    unsigned long AffinityMask;// Offset=0xc Size=0x4
};

struct _RTL_PROCESS_BACKTRACES// Size=0x9c (Id=603)
{
    unsigned long CommittedMemory;// Offset=0x0 Size=0x4
    unsigned long ReservedMemory;// Offset=0x4 Size=0x4
    unsigned long NumberOfBackTraceLookups;// Offset=0x8 Size=0x4
    unsigned long NumberOfBackTraces;// Offset=0xc Size=0x4
    struct _RTL_PROCESS_BACKTRACE_INFORMATION BackTraces[1];// Offset=0x10 Size=0x8c
};

struct _SECURITY_DESCRIPTOR_RELATIVE// Size=0x14 (Id=605)
{
    unsigned char Revision;// Offset=0x0 Size=0x1
    unsigned char Sbz1;// Offset=0x1 Size=0x1
    unsigned short Control;// Offset=0x2 Size=0x2
    unsigned long Owner;// Offset=0x4 Size=0x4
    unsigned long Group;// Offset=0x8 Size=0x4
    unsigned long Sacl;// Offset=0xc Size=0x4
    unsigned long Dacl;// Offset=0x10 Size=0x4
};

struct _FILE_NAMES_INFORMATION// Size=0x10 (Id=606)
{
    unsigned long NextEntryOffset;// Offset=0x0 Size=0x4
    unsigned long FileIndex;// Offset=0x4 Size=0x4
    unsigned long FileNameLength;// Offset=0x8 Size=0x4
    char FileName[1];// Offset=0xc Size=0x1
};

struct _RTL_RANGE_LIST// Size=0x14 (Id=607)
{
    struct _LIST_ENTRY ListHead;// Offset=0x0 Size=0x8
    unsigned long Flags;// Offset=0x8 Size=0x4
    unsigned long Count;// Offset=0xc Size=0x4
    unsigned long Stamp;// Offset=0x10 Size=0x4
};

struct _TOKEN_STATISTICS// Size=0x38 (Id=608)
{
    struct _LUID TokenId;// Offset=0x0 Size=0x8
    struct _LUID AuthenticationId;// Offset=0x8 Size=0x8
    union _LARGE_INTEGER ExpirationTime;// Offset=0x10 Size=0x8
    enum _TOKEN_TYPE TokenType;// Offset=0x18 Size=0x4
    enum _SECURITY_IMPERSONATION_LEVEL ImpersonationLevel;// Offset=0x1c Size=0x4
    unsigned long DynamicCharged;// Offset=0x20 Size=0x4
    unsigned long DynamicAvailable;// Offset=0x24 Size=0x4
    unsigned long GroupCount;// Offset=0x28 Size=0x4
    unsigned long PrivilegeCount;// Offset=0x2c Size=0x4
    struct _LUID ModifiedId;// Offset=0x30 Size=0x8
};

struct _FILE_EA_INFORMATION// Size=0x4 (Id=609)
{
    unsigned long EaSize;// Offset=0x0 Size=0x4
};

struct _KEY_VALUE_PARTIAL_INFORMATION_ALIGN64// Size=0xc (Id=611)
{
    unsigned long Type;// Offset=0x0 Size=0x4
    unsigned long DataLength;// Offset=0x4 Size=0x4
    unsigned char Data[1];// Offset=0x8 Size=0x1
};

struct _KEY_VALUE_BASIC_INFORMATION// Size=0x10 (Id=616)
{
    unsigned long TitleIndex;// Offset=0x0 Size=0x4
    unsigned long Type;// Offset=0x4 Size=0x4
    unsigned long NameLength;// Offset=0x8 Size=0x4
    unsigned short Name[1];// Offset=0xc Size=0x2
};

struct _IMAGE_RESOURCE_DATA_ENTRY// Size=0x10 (Id=625)
{
    unsigned long OffsetToData;// Offset=0x0 Size=0x4
    unsigned long Size;// Offset=0x4 Size=0x4
    unsigned long CodePage;// Offset=0x8 Size=0x4
    unsigned long Reserved;// Offset=0xc Size=0x4
};

struct _SYSTEM_PERFORMANCE_INFORMATION// Size=0x138 (Id=627)
{
    union _LARGE_INTEGER IdleProcessTime;// Offset=0x0 Size=0x8
    union _LARGE_INTEGER IoReadTransferCount;// Offset=0x8 Size=0x8
    union _LARGE_INTEGER IoWriteTransferCount;// Offset=0x10 Size=0x8
    union _LARGE_INTEGER IoOtherTransferCount;// Offset=0x18 Size=0x8
    unsigned long IoReadOperationCount;// Offset=0x20 Size=0x4
    unsigned long IoWriteOperationCount;// Offset=0x24 Size=0x4
    unsigned long IoOtherOperationCount;// Offset=0x28 Size=0x4
    unsigned long AvailablePages;// Offset=0x2c Size=0x4
    unsigned long CommittedPages;// Offset=0x30 Size=0x4
    unsigned long CommitLimit;// Offset=0x34 Size=0x4
    unsigned long PeakCommitment;// Offset=0x38 Size=0x4
    unsigned long PageFaultCount;// Offset=0x3c Size=0x4
    unsigned long CopyOnWriteCount;// Offset=0x40 Size=0x4
    unsigned long TransitionCount;// Offset=0x44 Size=0x4
    unsigned long CacheTransitionCount;// Offset=0x48 Size=0x4
    unsigned long DemandZeroCount;// Offset=0x4c Size=0x4
    unsigned long PageReadCount;// Offset=0x50 Size=0x4
    unsigned long PageReadIoCount;// Offset=0x54 Size=0x4
    unsigned long CacheReadCount;// Offset=0x58 Size=0x4
    unsigned long CacheIoCount;// Offset=0x5c Size=0x4
    unsigned long DirtyPagesWriteCount;// Offset=0x60 Size=0x4
    unsigned long DirtyWriteIoCount;// Offset=0x64 Size=0x4
    unsigned long MappedPagesWriteCount;// Offset=0x68 Size=0x4
    unsigned long MappedWriteIoCount;// Offset=0x6c Size=0x4
    unsigned long PagedPoolPages;// Offset=0x70 Size=0x4
    unsigned long NonPagedPoolPages;// Offset=0x74 Size=0x4
    unsigned long PagedPoolAllocs;// Offset=0x78 Size=0x4
    unsigned long PagedPoolFrees;// Offset=0x7c Size=0x4
    unsigned long NonPagedPoolAllocs;// Offset=0x80 Size=0x4
    unsigned long NonPagedPoolFrees;// Offset=0x84 Size=0x4
    unsigned long FreeSystemPtes;// Offset=0x88 Size=0x4
    unsigned long ResidentSystemCodePage;// Offset=0x8c Size=0x4
    unsigned long TotalSystemDriverPages;// Offset=0x90 Size=0x4
    unsigned long TotalSystemCodePages;// Offset=0x94 Size=0x4
    unsigned long NonPagedPoolLookasideHits;// Offset=0x98 Size=0x4
    unsigned long PagedPoolLookasideHits;// Offset=0x9c Size=0x4
    unsigned long Spare3Count;// Offset=0xa0 Size=0x4
    unsigned long ResidentSystemCachePage;// Offset=0xa4 Size=0x4
    unsigned long ResidentPagedPoolPage;// Offset=0xa8 Size=0x4
    unsigned long ResidentSystemDriverPage;// Offset=0xac Size=0x4
    unsigned long CcFastReadNoWait;// Offset=0xb0 Size=0x4
    unsigned long CcFastReadWait;// Offset=0xb4 Size=0x4
    unsigned long CcFastReadResourceMiss;// Offset=0xb8 Size=0x4
    unsigned long CcFastReadNotPossible;// Offset=0xbc Size=0x4
    unsigned long CcFastMdlReadNoWait;// Offset=0xc0 Size=0x4
    unsigned long CcFastMdlReadWait;// Offset=0xc4 Size=0x4
    unsigned long CcFastMdlReadResourceMiss;// Offset=0xc8 Size=0x4
    unsigned long CcFastMdlReadNotPossible;// Offset=0xcc Size=0x4
    unsigned long CcMapDataNoWait;// Offset=0xd0 Size=0x4
    unsigned long CcMapDataWait;// Offset=0xd4 Size=0x4
    unsigned long CcMapDataNoWaitMiss;// Offset=0xd8 Size=0x4
    unsigned long CcMapDataWaitMiss;// Offset=0xdc Size=0x4
    unsigned long CcPinMappedDataCount;// Offset=0xe0 Size=0x4
    unsigned long CcPinReadNoWait;// Offset=0xe4 Size=0x4
    unsigned long CcPinReadWait;// Offset=0xe8 Size=0x4
    unsigned long CcPinReadNoWaitMiss;// Offset=0xec Size=0x4
    unsigned long CcPinReadWaitMiss;// Offset=0xf0 Size=0x4
    unsigned long CcCopyReadNoWait;// Offset=0xf4 Size=0x4
    unsigned long CcCopyReadWait;// Offset=0xf8 Size=0x4
    unsigned long CcCopyReadNoWaitMiss;// Offset=0xfc Size=0x4
    unsigned long CcCopyReadWaitMiss;// Offset=0x100 Size=0x4
    unsigned long CcMdlReadNoWait;// Offset=0x104 Size=0x4
    unsigned long CcMdlReadWait;// Offset=0x108 Size=0x4
    unsigned long CcMdlReadNoWaitMiss;// Offset=0x10c Size=0x4
    unsigned long CcMdlReadWaitMiss;// Offset=0x110 Size=0x4
    unsigned long CcReadAheadIos;// Offset=0x114 Size=0x4
    unsigned long CcLazyWriteIos;// Offset=0x118 Size=0x4
    unsigned long CcLazyWritePages;// Offset=0x11c Size=0x4
    unsigned long CcDataFlushes;// Offset=0x120 Size=0x4
    unsigned long CcDataPages;// Offset=0x124 Size=0x4
    unsigned long ContextSwitches;// Offset=0x128 Size=0x4
    unsigned long FirstLevelTbFills;// Offset=0x12c Size=0x4
    unsigned long SecondLevelTbFills;// Offset=0x130 Size=0x4
    unsigned long SystemCalls;// Offset=0x134 Size=0x4
};

struct _KSPECIAL_REGISTERS// Size=0x54 (Id=629)
{
    unsigned long Cr0;// Offset=0x0 Size=0x4
    unsigned long Cr2;// Offset=0x4 Size=0x4
    unsigned long Cr3;// Offset=0x8 Size=0x4
    unsigned long Cr4;// Offset=0xc Size=0x4
    unsigned long KernelDr0;// Offset=0x10 Size=0x4
    unsigned long KernelDr1;// Offset=0x14 Size=0x4
    unsigned long KernelDr2;// Offset=0x18 Size=0x4
    unsigned long KernelDr3;// Offset=0x1c Size=0x4
    unsigned long KernelDr6;// Offset=0x20 Size=0x4
    unsigned long KernelDr7;// Offset=0x24 Size=0x4
    struct _DESCRIPTOR Gdtr;// Offset=0x28 Size=0x8
    struct _DESCRIPTOR Idtr;// Offset=0x30 Size=0x8
    unsigned short Tr;// Offset=0x38 Size=0x2
    unsigned short Ldtr;// Offset=0x3a Size=0x2
    unsigned long Reserved[6];// Offset=0x3c Size=0x18
};

struct _SYSTEM_EXCEPTION_INFORMATION// Size=0x10 (Id=630)
{
    unsigned long AlignmentFixupCount;// Offset=0x0 Size=0x4
    unsigned long ExceptionDispatchCount;// Offset=0x4 Size=0x4
    unsigned long FloatingEmulationCount;// Offset=0x8 Size=0x4
    unsigned long ByteWordEmulationCount;// Offset=0xc Size=0x4
};

struct _CM_FLOPPY_DEVICE_DATA// Size=0x24 (Id=631)
{
    unsigned short Version;// Offset=0x0 Size=0x2
    unsigned short Revision;// Offset=0x2 Size=0x2
    char Size[8];// Offset=0x4 Size=0x8
    unsigned long MaxDensity;// Offset=0xc Size=0x4
    unsigned long MountDensity;// Offset=0x10 Size=0x4
    unsigned char StepRateHeadUnloadTime;// Offset=0x14 Size=0x1
    unsigned char HeadLoadTime;// Offset=0x15 Size=0x1
    unsigned char MotorOffTime;// Offset=0x16 Size=0x1
    unsigned char SectorLengthCode;// Offset=0x17 Size=0x1
    unsigned char SectorPerTrack;// Offset=0x18 Size=0x1
    unsigned char ReadWriteGapLength;// Offset=0x19 Size=0x1
    unsigned char DataTransferLength;// Offset=0x1a Size=0x1
    unsigned char FormatGapLength;// Offset=0x1b Size=0x1
    unsigned char FormatFillCharacter;// Offset=0x1c Size=0x1
    unsigned char HeadSettleTime;// Offset=0x1d Size=0x1
    unsigned char MotorSettleTime;// Offset=0x1e Size=0x1
    unsigned char MaximumTrackValue;// Offset=0x1f Size=0x1
    unsigned char DataTransferRate;// Offset=0x20 Size=0x1
};

struct _IMAGE_ALPHA64_RUNTIME_FUNCTION_ENTRY// Size=0x28 (Id=633)
{
    unsigned long long BeginAddress;// Offset=0x0 Size=0x8
    unsigned long long EndAddress;// Offset=0x8 Size=0x8
    unsigned long long ExceptionHandler;// Offset=0x10 Size=0x8
    unsigned long long HandlerData;// Offset=0x18 Size=0x8
    unsigned long long PrologEndAddress;// Offset=0x20 Size=0x8
};

struct _FILE_RENAME_INFORMATION// Size=0x10 (Id=634)
{
    unsigned char ReplaceIfExists;// Offset=0x0 Size=0x1
    unsigned char __align0[3];// Offset=0x1 Size=0x3
    void * RootDirectory;// Offset=0x4 Size=0x4
    struct _STRING FileName;// Offset=0x8 Size=0x8
};

struct _IO_COMPLETION_CONTEXT// Size=0x8 (Id=636)
{
    void * Port;// Offset=0x0 Size=0x4
    void * Key;// Offset=0x4 Size=0x4
};

struct _RTL_PROCESS_MODULES// Size=0x120 (Id=642)
{
    unsigned long NumberOfModules;// Offset=0x0 Size=0x4
    struct _RTL_PROCESS_MODULE_INFORMATION Modules[1];// Offset=0x4 Size=0x11c
};

struct _RTL_USER_PROCESS_PARAMETERS// Size=0x0 (Id=643)
{
};

struct _SYSTEM_FLAGS_INFORMATION// Size=0x4 (Id=645)
{
    unsigned long Flags;// Offset=0x0 Size=0x4
};

struct _PROCESS_ACCESS_TOKEN// Size=0x8 (Id=648)
{
    void * Token;// Offset=0x0 Size=0x4
    void * Thread;// Offset=0x4 Size=0x4
};

struct _SYSTEM_ALARM_ACE// Size=0xc (Id=651)
{
    struct _ACE_HEADER Header;// Offset=0x0 Size=0x4
    unsigned long Mask;// Offset=0x4 Size=0x4
    unsigned long SidStart;// Offset=0x8 Size=0x4
};

struct _SYSTEM_MEMORY_INFO// Size=0xc (Id=653)
{
    unsigned char * StringOffset;// Offset=0x0 Size=0x4
    unsigned short ValidCount;// Offset=0x4 Size=0x2
    unsigned short TransitionCount;// Offset=0x6 Size=0x2
    unsigned short ModifiedCount;// Offset=0x8 Size=0x2
    unsigned short PageTableCount;// Offset=0xa Size=0x2
};

struct _SYSTEM_INTERRUPT_INFORMATION// Size=0x18 (Id=654)
{
    unsigned long ContextSwitches;// Offset=0x0 Size=0x4
    unsigned long DpcCount;// Offset=0x4 Size=0x4
    unsigned long DpcRate;// Offset=0x8 Size=0x4
    unsigned long TimeIncrement;// Offset=0xc Size=0x4
    unsigned long DpcBypassCount;// Offset=0x10 Size=0x4
    unsigned long ApcBypassCount;// Offset=0x14 Size=0x4
};

struct _RTL_RANGE// Size=0x20 (Id=657)
{
    unsigned long long Start;// Offset=0x0 Size=0x8
    unsigned long long End;// Offset=0x8 Size=0x8
    void * UserData;// Offset=0x10 Size=0x4
    void * Owner;// Offset=0x14 Size=0x4
    unsigned char Attributes;// Offset=0x18 Size=0x1
    unsigned char Flags;// Offset=0x19 Size=0x1
};

struct _IMAGE_BOUND_FORWARDER_REF// Size=0x8 (Id=659)
{
    unsigned long TimeDateStamp;// Offset=0x0 Size=0x4
    unsigned short OffsetModuleName;// Offset=0x4 Size=0x2
    unsigned short Reserved;// Offset=0x6 Size=0x2
};

struct _SECURITY_TOKEN_AUDIT_DATA// Size=0xc (Id=660)
{
    unsigned long Length;// Offset=0x0 Size=0x4
    unsigned long GrantMask;// Offset=0x4 Size=0x4
    unsigned long DenyMask;// Offset=0x8 Size=0x4
};

struct _TOKEN_OWNER// Size=0x4 (Id=662)
{
    void * Owner;// Offset=0x0 Size=0x4
};

struct _FILE_ALL_INFORMATION// Size=0x68 (Id=665)
{
    struct _FILE_BASIC_INFORMATION BasicInformation;// Offset=0x0 Size=0x28
    struct _FILE_STANDARD_INFORMATION StandardInformation;// Offset=0x28 Size=0x18
    struct _FILE_INTERNAL_INFORMATION InternalInformation;// Offset=0x40 Size=0x8
    struct _FILE_EA_INFORMATION EaInformation;// Offset=0x48 Size=0x4
    struct _FILE_ACCESS_INFORMATION AccessInformation;// Offset=0x4c Size=0x4
    struct _FILE_POSITION_INFORMATION PositionInformation;// Offset=0x50 Size=0x8
    struct _FILE_MODE_INFORMATION ModeInformation;// Offset=0x58 Size=0x4
    struct _FILE_ALIGNMENT_INFORMATION AlignmentInformation;// Offset=0x5c Size=0x4
    struct _FILE_NAME_INFORMATION NameInformation;// Offset=0x60 Size=0x8
};

struct _IMAGE_DOS_HEADER// Size=0x40 (Id=668)
{
    unsigned short e_magic;// Offset=0x0 Size=0x2
    unsigned short e_cblp;// Offset=0x2 Size=0x2
    unsigned short e_cp;// Offset=0x4 Size=0x2
    unsigned short e_crlc;// Offset=0x6 Size=0x2
    unsigned short e_cparhdr;// Offset=0x8 Size=0x2
    unsigned short e_minalloc;// Offset=0xa Size=0x2
    unsigned short e_maxalloc;// Offset=0xc Size=0x2
    unsigned short e_ss;// Offset=0xe Size=0x2
    unsigned short e_sp;// Offset=0x10 Size=0x2
    unsigned short e_csum;// Offset=0x12 Size=0x2
    unsigned short e_ip;// Offset=0x14 Size=0x2
    unsigned short e_cs;// Offset=0x16 Size=0x2
    unsigned short e_lfarlc;// Offset=0x18 Size=0x2
    unsigned short e_ovno;// Offset=0x1a Size=0x2
    unsigned short e_res[4];// Offset=0x1c Size=0x8
    unsigned short e_oemid;// Offset=0x24 Size=0x2
    unsigned short e_oeminfo;// Offset=0x26 Size=0x2
    unsigned short e_res2[10];// Offset=0x28 Size=0x14
    long e_lfanew;// Offset=0x3c Size=0x4
};

struct _CM_ROM_BLOCK// Size=0x8 (Id=674)
{
    unsigned long Address;// Offset=0x0 Size=0x4
    unsigned long Size;// Offset=0x4 Size=0x4
};

struct _SYSTEM_POWER_INFORMATION// Size=0x10 (Id=682)
{
    unsigned long MaxIdlenessAllowed;// Offset=0x0 Size=0x4
    unsigned long Idleness;// Offset=0x4 Size=0x4
    unsigned long TimeRemaining;// Offset=0x8 Size=0x4
    unsigned char CoolingMode;// Offset=0xc Size=0x1
};

struct _RTL_HEAP_PARAMETERS// Size=0x30 (Id=683)
{
    unsigned long Length;// Offset=0x0 Size=0x4
    unsigned long SegmentReserve;// Offset=0x4 Size=0x4
    unsigned long SegmentCommit;// Offset=0x8 Size=0x4
    unsigned long DeCommitFreeBlockThreshold;// Offset=0xc Size=0x4
    unsigned long DeCommitTotalFreeThreshold;// Offset=0x10 Size=0x4
    unsigned long MaximumAllocationSize;// Offset=0x14 Size=0x4
    unsigned long VirtualMemoryThreshold;// Offset=0x18 Size=0x4
    unsigned long InitialCommit;// Offset=0x1c Size=0x4
    unsigned long InitialReserve;// Offset=0x20 Size=0x4
    long  ( * CommitRoutine)(void * ,void ** ,unsigned long * );// Offset=0x24 Size=0x4
    unsigned long Reserved[2];// Offset=0x28 Size=0x8
};

struct _FILE_MODE_INFORMATION// Size=0x4 (Id=685)
{
    unsigned long Mode;// Offset=0x0 Size=0x4
};

struct _IMAGE_DEBUG_MISC// Size=0x10 (Id=690)
{
    unsigned long DataType;// Offset=0x0 Size=0x4
    unsigned long Length;// Offset=0x4 Size=0x4
    unsigned char Unicode;// Offset=0x8 Size=0x1
    unsigned char Reserved[3];// Offset=0x9 Size=0x3
    unsigned char Data[1];// Offset=0xc Size=0x1
};

struct _KPROCESS// Size=0x1c (Id=691)
{
    struct _LIST_ENTRY ReadyListHead;// Offset=0x0 Size=0x8
    struct _LIST_ENTRY ThreadListHead;// Offset=0x8 Size=0x8
    unsigned long StackCount;// Offset=0x10 Size=0x4
    long ThreadQuantum;// Offset=0x14 Size=0x4
    char BasePriority;// Offset=0x18 Size=0x1
    unsigned char DisableBoost;// Offset=0x19 Size=0x1
    unsigned char DisableQuantum;// Offset=0x1a Size=0x1
};

struct _FILE_ALIGNMENT_INFORMATION// Size=0x4 (Id=697)
{
    unsigned long AlignmentRequirement;// Offset=0x0 Size=0x4
};

struct _SYSTEM_CALL_COUNT_INFORMATION// Size=0x8 (Id=701)
{
    unsigned long Length;// Offset=0x0 Size=0x4
    unsigned long NumberOfTables;// Offset=0x4 Size=0x4
};

struct _FILE_BASIC_INFORMATION// Size=0x28 (Id=705)
{
    union _LARGE_INTEGER CreationTime;// Offset=0x0 Size=0x8
    union _LARGE_INTEGER LastAccessTime;// Offset=0x8 Size=0x8
    union _LARGE_INTEGER LastWriteTime;// Offset=0x10 Size=0x8
    union _LARGE_INTEGER ChangeTime;// Offset=0x18 Size=0x8
    unsigned long FileAttributes;// Offset=0x20 Size=0x4
};

struct _SYSTEM_QUERY_TIME_ADJUST_INFORMATION// Size=0xc (Id=706)
{
    unsigned long TimeAdjustment;// Offset=0x0 Size=0x4
    unsigned long TimeIncrement;// Offset=0x4 Size=0x4
    unsigned char Enable;// Offset=0x8 Size=0x1
};

struct _FILE_TRACKING_INFORMATION// Size=0xc (Id=708)
{
    void * DestinationFile;// Offset=0x0 Size=0x4
    unsigned long ObjectInformationLength;// Offset=0x4 Size=0x4
    char ObjectInformation[1];// Offset=0x8 Size=0x1
};

struct _RTL_CRITICAL_SECTION// Size=0x1c (Id=715)
{
    union __unnamed Synchronization;// Offset=0x0 Size=0x10
    long LockCount;// Offset=0x10 Size=0x4
    long RecursionCount;// Offset=0x14 Size=0x4
    void * OwningThread;// Offset=0x18 Size=0x4
};

struct _RTL_HEAP_INFORMATION// Size=0x40 (Id=716)
{
    void * BaseAddress;// Offset=0x0 Size=0x4
    unsigned long Flags;// Offset=0x4 Size=0x4
    unsigned short EntryOverhead;// Offset=0x8 Size=0x2
    unsigned short CreatorBackTraceIndex;// Offset=0xa Size=0x2
    unsigned long BytesAllocated;// Offset=0xc Size=0x4
    unsigned long BytesCommitted;// Offset=0x10 Size=0x4
    unsigned long NumberOfTags;// Offset=0x14 Size=0x4
    unsigned long NumberOfEntries;// Offset=0x18 Size=0x4
    unsigned long NumberOfPseudoTags;// Offset=0x1c Size=0x4
    unsigned long PseudoTagGranularity;// Offset=0x20 Size=0x4
    unsigned long Reserved[5];// Offset=0x24 Size=0x14
    struct _RTL_HEAP_TAG * Tags;// Offset=0x38 Size=0x4
    struct _RTL_HEAP_ENTRY * Entries;// Offset=0x3c Size=0x4
};

struct _RTL_HEAP_WALK_ENTRY// Size=0x1c (Id=719)
{
    void * DataAddress;// Offset=0x0 Size=0x4
    unsigned long DataSize;// Offset=0x4 Size=0x4
    unsigned char OverheadBytes;// Offset=0x8 Size=0x1
    unsigned char SegmentIndex;// Offset=0x9 Size=0x1
    unsigned short Flags;// Offset=0xa Size=0x2
    union // Size=0x10 (Id=0)
    {
        struct __unnamed Block;// Offset=0xc Size=0x10
        struct __unnamed Segment;// Offset=0xc Size=0x10
    };
};

struct _CM_SCSI_DEVICE_DATA// Size=0x6 (Id=730)
{
    unsigned short Version;// Offset=0x0 Size=0x2
    unsigned short Revision;// Offset=0x2 Size=0x2
    unsigned char HostIdentifier;// Offset=0x4 Size=0x1
};

struct _SYSTEM_AUDIT_ACE// Size=0xc (Id=732)
{
    struct _ACE_HEADER Header;// Offset=0x0 Size=0x4
    unsigned long Mask;// Offset=0x4 Size=0x4
    unsigned long SidStart;// Offset=0x8 Size=0x4
};

struct _FILE_STANDARD_INFORMATION// Size=0x18 (Id=733)
{
    union _LARGE_INTEGER AllocationSize;// Offset=0x0 Size=0x8
    union _LARGE_INTEGER EndOfFile;// Offset=0x8 Size=0x8
    unsigned long NumberOfLinks;// Offset=0x10 Size=0x4
    unsigned char DeletePending;// Offset=0x14 Size=0x1
    unsigned char Directory;// Offset=0x15 Size=0x1
};

struct _RTL_PROCESS_LOCKS// Size=0x28 (Id=740)
{
    unsigned long NumberOfLocks;// Offset=0x0 Size=0x4
    struct _RTL_PROCESS_LOCK_INFORMATION Locks[1];// Offset=0x4 Size=0x24
};

struct _RTL_TIME_ZONE_INFORMATION// Size=0xac (Id=741)
{
    long Bias;// Offset=0x0 Size=0x4
    unsigned short StandardName[32];// Offset=0x4 Size=0x40
    struct _TIME_FIELDS StandardStart;// Offset=0x44 Size=0x10
    long StandardBias;// Offset=0x54 Size=0x4
    unsigned short DaylightName[32];// Offset=0x58 Size=0x40
    struct _TIME_FIELDS DaylightStart;// Offset=0x98 Size=0x10
    long DaylightBias;// Offset=0xa8 Size=0x4
};

struct _SYSTEM_OBJECT_INFORMATION// Size=0x30 (Id=747)
{
    unsigned long NextEntryOffset;// Offset=0x0 Size=0x4
    void * Object;// Offset=0x4 Size=0x4
    void * CreatorUniqueProcess;// Offset=0x8 Size=0x4
    unsigned short CreatorBackTraceIndex;// Offset=0xc Size=0x2
    unsigned short Flags;// Offset=0xe Size=0x2
    long PointerCount;// Offset=0x10 Size=0x4
    long HandleCount;// Offset=0x14 Size=0x4
    unsigned long PagedPoolCharge;// Offset=0x18 Size=0x4
    unsigned long NonPagedPoolCharge;// Offset=0x1c Size=0x4
    void * ExclusiveProcessId;// Offset=0x20 Size=0x4
    void * SecurityDescriptor;// Offset=0x24 Size=0x4
    struct _OBJECT_NAME_INFORMATION NameInfo;// Offset=0x28 Size=0x8
};

struct _FILE_FS_SIZE_INFORMATION// Size=0x18 (Id=748)
{
    union _LARGE_INTEGER TotalAllocationUnits;// Offset=0x0 Size=0x8
    union _LARGE_INTEGER AvailableAllocationUnits;// Offset=0x8 Size=0x8
    unsigned long SectorsPerAllocationUnit;// Offset=0x10 Size=0x4
    unsigned long BytesPerSector;// Offset=0x14 Size=0x4
};

struct _KTRAP_FRAME// Size=0x50 (Id=749)
{
    unsigned long DbgEbp;// Offset=0x0 Size=0x4
    unsigned long DbgEip;// Offset=0x4 Size=0x4
    unsigned long DbgArgMark;// Offset=0x8 Size=0x4
    unsigned long DbgArgPointer;// Offset=0xc Size=0x4
    unsigned long TempSegCs;// Offset=0x10 Size=0x4
    unsigned long TempEsp;// Offset=0x14 Size=0x4
    unsigned long Edx;// Offset=0x18 Size=0x4
    unsigned long Ecx;// Offset=0x1c Size=0x4
    unsigned long Eax;// Offset=0x20 Size=0x4
    struct _EXCEPTION_REGISTRATION_RECORD * ExceptionList;// Offset=0x24 Size=0x4
    unsigned long Edi;// Offset=0x28 Size=0x4
    unsigned long Esi;// Offset=0x2c Size=0x4
    unsigned long Ebx;// Offset=0x30 Size=0x4
    unsigned long Ebp;// Offset=0x34 Size=0x4
    unsigned long ErrCode;// Offset=0x38 Size=0x4
    unsigned long Eip;// Offset=0x3c Size=0x4
    unsigned long SegCs;// Offset=0x40 Size=0x4
    unsigned long EFlags;// Offset=0x44 Size=0x4
    unsigned long HardwareEsp;// Offset=0x48 Size=0x4
    unsigned long HardwareSegSs;// Offset=0x4c Size=0x4
};

struct _SYSTEM_AUDIT_OBJECT_ACE// Size=0x30 (Id=751)
{
    struct _ACE_HEADER Header;// Offset=0x0 Size=0x4
    unsigned long Mask;// Offset=0x4 Size=0x4
    unsigned long Flags;// Offset=0x8 Size=0x4
    struct _GUID ObjectType;// Offset=0xc Size=0x10
    struct _GUID InheritedObjectType;// Offset=0x1c Size=0x10
    unsigned long SidStart;// Offset=0x2c Size=0x4
};

struct _FILE_PIPE_REMOTE_INFORMATION// Size=0x10 (Id=763)
{
    union _LARGE_INTEGER CollectDataTime;// Offset=0x0 Size=0x8
    unsigned long MaximumCollectionCount;// Offset=0x8 Size=0x4
};

struct _SYSTEM_REGISTRY_QUOTA_INFORMATION// Size=0xc (Id=768)
{
    unsigned long RegistryQuotaAllowed;// Offset=0x0 Size=0x4
    unsigned long RegistryQuotaUsed;// Offset=0x4 Size=0x4
    unsigned long PagedPoolSize;// Offset=0x8 Size=0x4
};

struct _PROCESSOR_POWER_INFORMATION// Size=0x18 (Id=769)
{
    unsigned long Number;// Offset=0x0 Size=0x4
    unsigned long MaxMhz;// Offset=0x4 Size=0x4
    unsigned long CurrentMhz;// Offset=0x8 Size=0x4
    unsigned long MhzLimit;// Offset=0xc Size=0x4
    unsigned long MaxIdleState;// Offset=0x10 Size=0x4
    unsigned long CurrentIdleState;// Offset=0x14 Size=0x4
};

struct _SYSTEM_POWER_POLICY// Size=0xe8 (Id=774)
{
    unsigned long Revision;// Offset=0x0 Size=0x4
    struct POWER_ACTION_POLICY PowerButton;// Offset=0x4 Size=0xc
    struct POWER_ACTION_POLICY SleepButton;// Offset=0x10 Size=0xc
    struct POWER_ACTION_POLICY LidClose;// Offset=0x1c Size=0xc
    enum _SYSTEM_POWER_STATE LidOpenWake;// Offset=0x28 Size=0x4
    unsigned long Reserved;// Offset=0x2c Size=0x4
    struct POWER_ACTION_POLICY Idle;// Offset=0x30 Size=0xc
    unsigned long IdleTimeout;// Offset=0x3c Size=0x4
    unsigned char IdleSensitivity;// Offset=0x40 Size=0x1
    unsigned char Spare2[3];// Offset=0x41 Size=0x3
    enum _SYSTEM_POWER_STATE MinSleep;// Offset=0x44 Size=0x4
    enum _SYSTEM_POWER_STATE MaxSleep;// Offset=0x48 Size=0x4
    enum _SYSTEM_POWER_STATE ReducedLatencySleep;// Offset=0x4c Size=0x4
    unsigned long WinLogonFlags;// Offset=0x50 Size=0x4
    unsigned long Spare3;// Offset=0x54 Size=0x4
    unsigned long DozeS4Timeout;// Offset=0x58 Size=0x4
    unsigned long BroadcastCapacityResolution;// Offset=0x5c Size=0x4
    struct SYSTEM_POWER_LEVEL DischargePolicy[4];// Offset=0x60 Size=0x60
    unsigned long VideoTimeout;// Offset=0xc0 Size=0x4
    unsigned char VideoDimDisplay;// Offset=0xc4 Size=0x1
    unsigned char __align0[3];// Offset=0xc5 Size=0x3
    unsigned long VideoReserved[3];// Offset=0xc8 Size=0xc
    unsigned long SpindownTimeout;// Offset=0xd4 Size=0x4
    unsigned char OptimizeForPower;// Offset=0xd8 Size=0x1
    unsigned char FanThrottleTolerance;// Offset=0xd9 Size=0x1
    unsigned char ForcedThrottle;// Offset=0xda Size=0x1
    unsigned char MinThrottle;// Offset=0xdb Size=0x1
    struct POWER_ACTION_POLICY OverThrottled;// Offset=0xdc Size=0xc
};

struct _FILE_DIRECTORY_INFORMATION// Size=0x48 (Id=775)
{
    unsigned long NextEntryOffset;// Offset=0x0 Size=0x4
    unsigned long FileIndex;// Offset=0x4 Size=0x4
    union _LARGE_INTEGER CreationTime;// Offset=0x8 Size=0x8
    union _LARGE_INTEGER LastAccessTime;// Offset=0x10 Size=0x8
    union _LARGE_INTEGER LastWriteTime;// Offset=0x18 Size=0x8
    union _LARGE_INTEGER ChangeTime;// Offset=0x20 Size=0x8
    union _LARGE_INTEGER EndOfFile;// Offset=0x28 Size=0x8
    union _LARGE_INTEGER AllocationSize;// Offset=0x30 Size=0x8
    unsigned long FileAttributes;// Offset=0x38 Size=0x4
    unsigned long FileNameLength;// Offset=0x3c Size=0x4
    char FileName[1];// Offset=0x40 Size=0x1
};

struct _IMAGE_FUNCTION_ENTRY// Size=0xc (Id=781)
{
    unsigned long StartingAddress;// Offset=0x0 Size=0x4
    unsigned long EndingAddress;// Offset=0x4 Size=0x4
    unsigned long EndOfPrologue;// Offset=0x8 Size=0x4
};

struct _PROCESS_BASIC_INFORMATION// Size=0x18 (Id=782)
{
    long ExitStatus;// Offset=0x0 Size=0x4
    struct _PEB * PebBaseAddress;// Offset=0x4 Size=0x4
    unsigned long AffinityMask;// Offset=0x8 Size=0x4
    long BasePriority;// Offset=0xc Size=0x4
    unsigned long UniqueProcessId;// Offset=0x10 Size=0x4
    unsigned long InheritedFromUniqueProcessId;// Offset=0x14 Size=0x4
};

struct _CM_PCCARD_DEVICE_DATA// Size=0x20 (Id=784)
{
    unsigned char Flags;// Offset=0x0 Size=0x1
    unsigned char ErrorCode;// Offset=0x1 Size=0x1
    unsigned short Reserved;// Offset=0x2 Size=0x2
    unsigned long BusData;// Offset=0x4 Size=0x4
    unsigned long DeviceId;// Offset=0x8 Size=0x4
    unsigned long LegacyBaseAddress;// Offset=0xc Size=0x4
    unsigned char IRQMap[16];// Offset=0x10 Size=0x10
};

struct _IMAGE_SECTION_HEADER// Size=0x28 (Id=787)
{
    unsigned char Name[8];// Offset=0x0 Size=0x8
    union __unnamed Misc;// Offset=0x8 Size=0x4
    unsigned long VirtualAddress;// Offset=0xc Size=0x4
    unsigned long SizeOfRawData;// Offset=0x10 Size=0x4
    unsigned long PointerToRawData;// Offset=0x14 Size=0x4
    unsigned long PointerToRelocations;// Offset=0x18 Size=0x4
    unsigned long PointerToLinenumbers;// Offset=0x1c Size=0x4
    unsigned short NumberOfRelocations;// Offset=0x20 Size=0x2
    unsigned short NumberOfLinenumbers;// Offset=0x22 Size=0x2
    unsigned long Characteristics;// Offset=0x24 Size=0x4
};

struct _CM_PARTIAL_RESOURCE_DESCRIPTOR// Size=0x10 (Id=795)
{
    unsigned char Type;// Offset=0x0 Size=0x1
    unsigned char ShareDisposition;// Offset=0x1 Size=0x1
    unsigned short Flags;// Offset=0x2 Size=0x2
    union __unnamed u;// Offset=0x4 Size=0xc
};

struct _KGDTENTRY// Size=0x8 (Id=805)
{
    unsigned short LimitLow;// Offset=0x0 Size=0x2
    unsigned short BaseLow;// Offset=0x2 Size=0x2
    union __unnamed HighWord;// Offset=0x4 Size=0x4
};

struct _PROCESS_SESSION_INFORMATION// Size=0x4 (Id=806)
{
    unsigned long SessionId;// Offset=0x0 Size=0x4
};

struct _SYSTEM_CONTEXT_SWITCH_INFORMATION// Size=0x30 (Id=807)
{
    unsigned long ContextSwitches;// Offset=0x0 Size=0x4
    unsigned long FindAny;// Offset=0x4 Size=0x4
    unsigned long FindLast;// Offset=0x8 Size=0x4
    unsigned long FindIdeal;// Offset=0xc Size=0x4
    unsigned long IdleAny;// Offset=0x10 Size=0x4
    unsigned long IdleCurrent;// Offset=0x14 Size=0x4
    unsigned long IdleLast;// Offset=0x18 Size=0x4
    unsigned long IdleIdeal;// Offset=0x1c Size=0x4
    unsigned long PreemptAny;// Offset=0x20 Size=0x4
    unsigned long PreemptCurrent;// Offset=0x24 Size=0x4
    unsigned long PreemptLast;// Offset=0x28 Size=0x4
    unsigned long SwitchToIdle;// Offset=0x2c Size=0x4
};

struct _SYSTEM_DEVICE_INFORMATION// Size=0x18 (Id=808)
{
    unsigned long NumberOfDisks;// Offset=0x0 Size=0x4
    unsigned long NumberOfFloppies;// Offset=0x4 Size=0x4
    unsigned long NumberOfCdRoms;// Offset=0x8 Size=0x4
    unsigned long NumberOfTapes;// Offset=0xc Size=0x4
    unsigned long NumberOfSerialPorts;// Offset=0x10 Size=0x4
    unsigned long NumberOfParallelPorts;// Offset=0x14 Size=0x4
};

struct _IMAGE_NT_HEADERS64// Size=0x108 (Id=812)
{
    unsigned long Signature;// Offset=0x0 Size=0x4
    struct _IMAGE_FILE_HEADER FileHeader;// Offset=0x4 Size=0x14
    struct _IMAGE_OPTIONAL_HEADER64 OptionalHeader;// Offset=0x18 Size=0xf0
};

struct _FILE_NAME_INFORMATION// Size=0x8 (Id=814)
{
    unsigned long FileNameLength;// Offset=0x0 Size=0x4
    char FileName[1];// Offset=0x4 Size=0x1
};

struct _FILE_COMPRESSION_INFORMATION// Size=0x10 (Id=819)
{
    union _LARGE_INTEGER CompressedFileSize;// Offset=0x0 Size=0x8
    unsigned short CompressionFormat;// Offset=0x8 Size=0x2
    unsigned char CompressionUnitShift;// Offset=0xa Size=0x1
    unsigned char ChunkShift;// Offset=0xb Size=0x1
    unsigned char ClusterShift;// Offset=0xc Size=0x1
    unsigned char Reserved[3];// Offset=0xd Size=0x3
};

struct _KEY_WRITE_TIME_INFORMATION// Size=0x8 (Id=821)
{
    union _LARGE_INTEGER LastWriteTime;// Offset=0x0 Size=0x8
};

struct _KSWITCHFRAME// Size=0xc (Id=827)
{
    unsigned long ExceptionList;// Offset=0x0 Size=0x4
    unsigned long Eflags;// Offset=0x4 Size=0x4
    unsigned long RetAddr;// Offset=0x8 Size=0x4
};

struct _SECURITY_DESCRIPTOR// Size=0x14 (Id=828)
{
    unsigned char Revision;// Offset=0x0 Size=0x1
    unsigned char Sbz1;// Offset=0x1 Size=0x1
    unsigned short Control;// Offset=0x2 Size=0x2
    void * Owner;// Offset=0x4 Size=0x4
    void * Group;// Offset=0x8 Size=0x4
    struct _ACL * Sacl;// Offset=0xc Size=0x4
    struct _ACL * Dacl;// Offset=0x10 Size=0x4
};

struct _IMAGE_RESOURCE_DIR_STRING_U// Size=0x4 (Id=830)
{
    unsigned short Length;// Offset=0x0 Size=0x2
    unsigned short NameString[1];// Offset=0x2 Size=0x2
};

struct _KIDTENTRY// Size=0x8 (Id=831)
{
    unsigned short Offset;// Offset=0x0 Size=0x2
    unsigned short Selector;// Offset=0x2 Size=0x2
    unsigned short Access;// Offset=0x4 Size=0x2
    unsigned short ExtendedOffset;// Offset=0x6 Size=0x2
};

struct _CM_PNP_BIOS_DEVICE_NODE// Size=0xc (Id=836)
{
    unsigned short Size;// Offset=0x0 Size=0x2
    unsigned char Node;// Offset=0x2 Size=0x1
    unsigned long ProductId;// Offset=0x3 Size=0x4
    unsigned char DeviceType[3];// Offset=0x7 Size=0x3
    unsigned short DeviceAttributes;// Offset=0xa Size=0x2
};

struct _FILE_POSITION_INFORMATION// Size=0x8 (Id=837)
{
    union _LARGE_INTEGER CurrentByteOffset;// Offset=0x0 Size=0x8
};

struct _PROCESS_DEVICEMAP_INFORMATION// Size=0x24 (Id=840)
{
    union // Size=0x4 (Id=0)
    {
        struct __unnamed Set;// Offset=0x0 Size=0x4
        struct __unnamed Query;// Offset=0x0 Size=0x24
    };
};

struct _TOKEN_PRIMARY_GROUP// Size=0x4 (Id=843)
{
    void * PrimaryGroup;// Offset=0x0 Size=0x4
};

struct _SECURITY_QUALITY_OF_SERVICE// Size=0xc (Id=845)
{
    unsigned long Length;// Offset=0x0 Size=0x4
    enum _SECURITY_IMPERSONATION_LEVEL ImpersonationLevel;// Offset=0x4 Size=0x4
    unsigned char ContextTrackingMode;// Offset=0x8 Size=0x1
    unsigned char EffectiveOnly;// Offset=0x9 Size=0x1
};

struct _FILE_OBJECT// Size=0x48 (Id=846)
{
    short Type;// Offset=0x0 Size=0x2
    struct // Size=0x1 (Id=0)
    {
        unsigned char DeletePending:1;// Offset=0x2 Size=0x1 BitOffset=0x0 BitSize=0x1
        unsigned char ReadAccess:1;// Offset=0x2 Size=0x1 BitOffset=0x1 BitSize=0x1
        unsigned char WriteAccess:1;// Offset=0x2 Size=0x1 BitOffset=0x2 BitSize=0x1
        unsigned char DeleteAccess:1;// Offset=0x2 Size=0x1 BitOffset=0x3 BitSize=0x1
        unsigned char SharedRead:1;// Offset=0x2 Size=0x1 BitOffset=0x4 BitSize=0x1
        unsigned char SharedWrite:1;// Offset=0x2 Size=0x1 BitOffset=0x5 BitSize=0x1
        unsigned char SharedDelete:1;// Offset=0x2 Size=0x1 BitOffset=0x6 BitSize=0x1
        unsigned char Reserved:1;// Offset=0x2 Size=0x1 BitOffset=0x7 BitSize=0x1
    };
    unsigned char Flags;// Offset=0x3 Size=0x1
    struct _DEVICE_OBJECT * DeviceObject;// Offset=0x4 Size=0x4
    void * FsContext;// Offset=0x8 Size=0x4
    void * FsContext2;// Offset=0xc Size=0x4
    long FinalStatus;// Offset=0x10 Size=0x4
    union _LARGE_INTEGER CurrentByteOffset;// Offset=0x14 Size=0x8
    struct _FILE_OBJECT * RelatedFileObject;// Offset=0x1c Size=0x4
    struct _IO_COMPLETION_CONTEXT * CompletionContext;// Offset=0x20 Size=0x4
    long LockCount;// Offset=0x24 Size=0x4
    struct _KEVENT Lock;// Offset=0x28 Size=0x10
    struct _KEVENT Event;// Offset=0x38 Size=0x10
};

struct _CM_MONITOR_DEVICE_DATA// Size=0x36 (Id=847)
{
    unsigned short Version;// Offset=0x0 Size=0x2
    unsigned short Revision;// Offset=0x2 Size=0x2
    unsigned short HorizontalScreenSize;// Offset=0x4 Size=0x2
    unsigned short VerticalScreenSize;// Offset=0x6 Size=0x2
    unsigned short HorizontalResolution;// Offset=0x8 Size=0x2
    unsigned short VerticalResolution;// Offset=0xa Size=0x2
    unsigned short HorizontalDisplayTimeLow;// Offset=0xc Size=0x2
    unsigned short HorizontalDisplayTime;// Offset=0xe Size=0x2
    unsigned short HorizontalDisplayTimeHigh;// Offset=0x10 Size=0x2
    unsigned short HorizontalBackPorchLow;// Offset=0x12 Size=0x2
    unsigned short HorizontalBackPorch;// Offset=0x14 Size=0x2
    unsigned short HorizontalBackPorchHigh;// Offset=0x16 Size=0x2
    unsigned short HorizontalFrontPorchLow;// Offset=0x18 Size=0x2
    unsigned short HorizontalFrontPorch;// Offset=0x1a Size=0x2
    unsigned short HorizontalFrontPorchHigh;// Offset=0x1c Size=0x2
    unsigned short HorizontalSyncLow;// Offset=0x1e Size=0x2
    unsigned short HorizontalSync;// Offset=0x20 Size=0x2
    unsigned short HorizontalSyncHigh;// Offset=0x22 Size=0x2
    unsigned short VerticalBackPorchLow;// Offset=0x24 Size=0x2
    unsigned short VerticalBackPorch;// Offset=0x26 Size=0x2
    unsigned short VerticalBackPorchHigh;// Offset=0x28 Size=0x2
    unsigned short VerticalFrontPorchLow;// Offset=0x2a Size=0x2
    unsigned short VerticalFrontPorch;// Offset=0x2c Size=0x2
    unsigned short VerticalFrontPorchHigh;// Offset=0x2e Size=0x2
    unsigned short VerticalSyncLow;// Offset=0x30 Size=0x2
    unsigned short VerticalSync;// Offset=0x32 Size=0x2
    unsigned short VerticalSyncHigh;// Offset=0x34 Size=0x2
};

struct _IMAGE_RESOURCE_DIRECTORY_STRING// Size=0x4 (Id=849)
{
    unsigned short Length;// Offset=0x0 Size=0x2
    char NameString[1];// Offset=0x2 Size=0x1
};

struct _PORT_DATA_INFORMATION// Size=0xc (Id=850)
{
    unsigned long CountDataEntries;// Offset=0x0 Size=0x4
    struct _PORT_DATA_ENTRY DataEntries[1];// Offset=0x4 Size=0x8
};

struct _PROCESS_WS_WATCH_INFORMATION// Size=0x8 (Id=852)
{
    void * FaultingPc;// Offset=0x0 Size=0x4
    void * FaultingVa;// Offset=0x4 Size=0x4
};

struct _RTL_HEAP_USAGE// Size=0x40 (Id=859)
{
    unsigned long Length;// Offset=0x0 Size=0x4
    unsigned long BytesAllocated;// Offset=0x4 Size=0x4
    unsigned long BytesCommitted;// Offset=0x8 Size=0x4
    unsigned long BytesReserved;// Offset=0xc Size=0x4
    unsigned long BytesReservedMaximum;// Offset=0x10 Size=0x4
    struct _RTL_HEAP_USAGE_ENTRY * Entries;// Offset=0x14 Size=0x4
    struct _RTL_HEAP_USAGE_ENTRY * AddedEntries;// Offset=0x18 Size=0x4
    struct _RTL_HEAP_USAGE_ENTRY * RemovedEntries;// Offset=0x1c Size=0x4
    unsigned long Reserved[8];// Offset=0x20 Size=0x20
};

struct _KEY_FULL_INFORMATION// Size=0x30 (Id=861)
{
    union _LARGE_INTEGER LastWriteTime;// Offset=0x0 Size=0x8
    unsigned long TitleIndex;// Offset=0x8 Size=0x4
    unsigned long ClassOffset;// Offset=0xc Size=0x4
    unsigned long ClassLength;// Offset=0x10 Size=0x4
    unsigned long SubKeys;// Offset=0x14 Size=0x4
    unsigned long MaxNameLen;// Offset=0x18 Size=0x4
    unsigned long MaxClassLen;// Offset=0x1c Size=0x4
    unsigned long Values;// Offset=0x20 Size=0x4
    unsigned long MaxValueNameLen;// Offset=0x24 Size=0x4
    unsigned long MaxValueDataLen;// Offset=0x28 Size=0x4
    unsigned short Class[1];// Offset=0x2c Size=0x2
};

struct _SYSTEM_BASIC_INFORMATION// Size=0x2c (Id=864)
{
    unsigned long Reserved;// Offset=0x0 Size=0x4
    unsigned long TimerResolution;// Offset=0x4 Size=0x4
    unsigned long PageSize;// Offset=0x8 Size=0x4
    unsigned long NumberOfPhysicalPages;// Offset=0xc Size=0x4
    unsigned long LowestPhysicalPageNumber;// Offset=0x10 Size=0x4
    unsigned long HighestPhysicalPageNumber;// Offset=0x14 Size=0x4
    unsigned long AllocationGranularity;// Offset=0x18 Size=0x4
    unsigned long MinimumUserModeAddress;// Offset=0x1c Size=0x4
    unsigned long MaximumUserModeAddress;// Offset=0x20 Size=0x4
    unsigned long ActiveProcessorsAffinityMask;// Offset=0x24 Size=0x4
    char NumberOfProcessors;// Offset=0x28 Size=0x1
};

struct _KiIoAccessMap// Size=0x2024 (Id=868)
{
    unsigned char DirectionMap[32];// Offset=0x0 Size=0x20
    unsigned char IoMap[8196];// Offset=0x20 Size=0x2004
};

struct _CM_FULL_RESOURCE_DESCRIPTOR// Size=0x20 (Id=871)
{
    enum _INTERFACE_TYPE InterfaceType;// Offset=0x0 Size=0x4
    unsigned long BusNumber;// Offset=0x4 Size=0x4
    struct _CM_PARTIAL_RESOURCE_LIST PartialResourceList;// Offset=0x8 Size=0x18
};

struct _TOKEN_PRIVILEGES// Size=0x10 (Id=872)
{
    unsigned long PrivilegeCount;// Offset=0x0 Size=0x4
    struct _LUID_AND_ATTRIBUTES Privileges[1];// Offset=0x4 Size=0xc
};

struct _PORT_MESSAGE// Size=0x18 (Id=877)
{
    union __unnamed u1;// Offset=0x0 Size=0x4
    union __unnamed u2;// Offset=0x4 Size=0x4
    union // Size=0x8 (Id=0)
    {
        struct _CLIENT_ID ClientId;// Offset=0x8 Size=0x8
        float DoNotUseThisField;// Offset=0x8 Size=0x8
    };
    unsigned long MessageId;// Offset=0x10 Size=0x4
    union // Size=0x4 (Id=0)
    {
        unsigned long ClientViewSize;// Offset=0x14 Size=0x4
        unsigned long CallbackId;// Offset=0x14 Size=0x4
    };
};

struct _FILE_FS_ATTRIBUTE_INFORMATION// Size=0x10 (Id=878)
{
    unsigned long FileSystemAttributes;// Offset=0x0 Size=0x4
    long MaximumComponentNameLength;// Offset=0x4 Size=0x4
    unsigned long FileSystemNameLength;// Offset=0x8 Size=0x4
    char FileSystemName[1];// Offset=0xc Size=0x1
};

struct _SYSTEM_VDM_INSTEMUL_INFO// Size=0x88 (Id=879)
{
    unsigned long SegmentNotPresent;// Offset=0x0 Size=0x4
    unsigned long VdmOpcode0F;// Offset=0x4 Size=0x4
    unsigned long OpcodeESPrefix;// Offset=0x8 Size=0x4
    unsigned long OpcodeCSPrefix;// Offset=0xc Size=0x4
    unsigned long OpcodeSSPrefix;// Offset=0x10 Size=0x4
    unsigned long OpcodeDSPrefix;// Offset=0x14 Size=0x4
    unsigned long OpcodeFSPrefix;// Offset=0x18 Size=0x4
    unsigned long OpcodeGSPrefix;// Offset=0x1c Size=0x4
    unsigned long OpcodeOPER32Prefix;// Offset=0x20 Size=0x4
    unsigned long OpcodeADDR32Prefix;// Offset=0x24 Size=0x4
    unsigned long OpcodeINSB;// Offset=0x28 Size=0x4
    unsigned long OpcodeINSW;// Offset=0x2c Size=0x4
    unsigned long OpcodeOUTSB;// Offset=0x30 Size=0x4
    unsigned long OpcodeOUTSW;// Offset=0x34 Size=0x4
    unsigned long OpcodePUSHF;// Offset=0x38 Size=0x4
    unsigned long OpcodePOPF;// Offset=0x3c Size=0x4
    unsigned long OpcodeINTnn;// Offset=0x40 Size=0x4
    unsigned long OpcodeINTO;// Offset=0x44 Size=0x4
    unsigned long OpcodeIRET;// Offset=0x48 Size=0x4
    unsigned long OpcodeINBimm;// Offset=0x4c Size=0x4
    unsigned long OpcodeINWimm;// Offset=0x50 Size=0x4
    unsigned long OpcodeOUTBimm;// Offset=0x54 Size=0x4
    unsigned long OpcodeOUTWimm;// Offset=0x58 Size=0x4
    unsigned long OpcodeINB;// Offset=0x5c Size=0x4
    unsigned long OpcodeINW;// Offset=0x60 Size=0x4
    unsigned long OpcodeOUTB;// Offset=0x64 Size=0x4
    unsigned long OpcodeOUTW;// Offset=0x68 Size=0x4
    unsigned long OpcodeLOCKPrefix;// Offset=0x6c Size=0x4
    unsigned long OpcodeREPNEPrefix;// Offset=0x70 Size=0x4
    unsigned long OpcodeREPPrefix;// Offset=0x74 Size=0x4
    unsigned long OpcodeHLT;// Offset=0x78 Size=0x4
    unsigned long OpcodeCLI;// Offset=0x7c Size=0x4
    unsigned long OpcodeSTI;// Offset=0x80 Size=0x4
    unsigned long BopCount;// Offset=0x84 Size=0x4
};

struct _IO_STACK_LOCATION// Size=0x24 (Id=893)
{
    unsigned char MajorFunction;// Offset=0x0 Size=0x1
    unsigned char MinorFunction;// Offset=0x1 Size=0x1
    unsigned char Flags;// Offset=0x2 Size=0x1
    unsigned char Control;// Offset=0x3 Size=0x1
    union __unnamed Parameters;// Offset=0x4 Size=0x10
    struct _DEVICE_OBJECT * DeviceObject;// Offset=0x14 Size=0x4
    struct _FILE_OBJECT * FileObject;// Offset=0x18 Size=0x4
    long  ( * CompletionRoutine)(struct _DEVICE_OBJECT * ,struct _IRP * ,void * );// Offset=0x1c Size=0x4
    void * Context;// Offset=0x20 Size=0x4
};

struct _SYSTEM_DPC_BEHAVIOR_INFORMATION// Size=0x14 (Id=897)
{
    unsigned long Spare;// Offset=0x0 Size=0x4
    unsigned long DpcQueueDepth;// Offset=0x4 Size=0x4
    unsigned long MinimumDpcRate;// Offset=0x8 Size=0x4
    unsigned long AdjustDpcThreshold;// Offset=0xc Size=0x4
    unsigned long IdealDpcRate;// Offset=0x10 Size=0x4
};

struct _RTL_GENERIC_TABLE// Size=0x28 (Id=900)
{
    struct _RTL_SPLAY_LINKS * TableRoot;// Offset=0x0 Size=0x4
    struct _LIST_ENTRY InsertOrderList;// Offset=0x4 Size=0x8
    struct _LIST_ENTRY * OrderedPointer;// Offset=0xc Size=0x4
    unsigned long WhichOrderedElement;// Offset=0x10 Size=0x4
    unsigned long NumberGenericTableElements;// Offset=0x14 Size=0x4
    enum _RTL_GENERIC_COMPARE_RESULTS  ( * CompareRoutine)(struct _RTL_GENERIC_TABLE * ,void * ,void * );// Offset=0x18 Size=0x4
    void *  ( * AllocateRoutine)(struct _RTL_GENERIC_TABLE * ,unsigned long );// Offset=0x1c Size=0x4
    void  ( * FreeRoutine)(struct _RTL_GENERIC_TABLE * ,void * );// Offset=0x20 Size=0x4
    void * TableContext;// Offset=0x24 Size=0x4
};

struct _SYSTEM_LOOKASIDE_INFORMATION// Size=0x20 (Id=903)
{
    unsigned short CurrentDepth;// Offset=0x0 Size=0x2
    unsigned short MaximumDepth;// Offset=0x2 Size=0x2
    unsigned long TotalAllocates;// Offset=0x4 Size=0x4
    unsigned long AllocateMisses;// Offset=0x8 Size=0x4
    unsigned long TotalFrees;// Offset=0xc Size=0x4
    unsigned long FreeMisses;// Offset=0x10 Size=0x4
    unsigned long Type;// Offset=0x14 Size=0x4
    unsigned long Tag;// Offset=0x18 Size=0x4
    unsigned long Size;// Offset=0x1c Size=0x4
};

struct _SYSTEM_FILECACHE_INFORMATION// Size=0x24 (Id=905)
{
    unsigned long CurrentSize;// Offset=0x0 Size=0x4
    unsigned long PeakSize;// Offset=0x4 Size=0x4
    unsigned long PageFaultCount;// Offset=0x8 Size=0x4
    unsigned long MinimumWorkingSet;// Offset=0xc Size=0x4
    unsigned long MaximumWorkingSet;// Offset=0x10 Size=0x4
    unsigned long CurrentSizeIncludingTransitionInPages;// Offset=0x14 Size=0x4
    unsigned long PeakSizeIncludingTransitionInPages;// Offset=0x18 Size=0x4
    unsigned long spare[2];// Offset=0x1c Size=0x8
};

struct _IMAGE_OS2_HEADER// Size=0x40 (Id=906)
{
    unsigned short ne_magic;// Offset=0x0 Size=0x2
    char ne_ver;// Offset=0x2 Size=0x1
    char ne_rev;// Offset=0x3 Size=0x1
    unsigned short ne_enttab;// Offset=0x4 Size=0x2
    unsigned short ne_cbenttab;// Offset=0x6 Size=0x2
    long ne_crc;// Offset=0x8 Size=0x4
    unsigned short ne_flags;// Offset=0xc Size=0x2
    unsigned short ne_autodata;// Offset=0xe Size=0x2
    unsigned short ne_heap;// Offset=0x10 Size=0x2
    unsigned short ne_stack;// Offset=0x12 Size=0x2
    long ne_csip;// Offset=0x14 Size=0x4
    long ne_sssp;// Offset=0x18 Size=0x4
    unsigned short ne_cseg;// Offset=0x1c Size=0x2
    unsigned short ne_cmod;// Offset=0x1e Size=0x2
    unsigned short ne_cbnrestab;// Offset=0x20 Size=0x2
    unsigned short ne_segtab;// Offset=0x22 Size=0x2
    unsigned short ne_rsrctab;// Offset=0x24 Size=0x2
    unsigned short ne_restab;// Offset=0x26 Size=0x2
    unsigned short ne_modtab;// Offset=0x28 Size=0x2
    unsigned short ne_imptab;// Offset=0x2a Size=0x2
    long ne_nrestab;// Offset=0x2c Size=0x4
    unsigned short ne_cmovent;// Offset=0x30 Size=0x2
    unsigned short ne_align;// Offset=0x32 Size=0x2
    unsigned short ne_cres;// Offset=0x34 Size=0x2
    unsigned char ne_exetyp;// Offset=0x36 Size=0x1
    unsigned char ne_flagsothers;// Offset=0x37 Size=0x1
    unsigned short ne_pretthunks;// Offset=0x38 Size=0x2
    unsigned short ne_psegrefbytes;// Offset=0x3a Size=0x2
    unsigned short ne_swaparea;// Offset=0x3c Size=0x2
    unsigned short ne_expver;// Offset=0x3e Size=0x2
};

struct _FILE_FS_DEVICE_INFORMATION// Size=0x8 (Id=907)
{
    unsigned long DeviceType;// Offset=0x0 Size=0x4
    unsigned long Characteristics;// Offset=0x4 Size=0x4
};

struct _IMAGE_TLS_DIRECTORY32// Size=0x18 (Id=909)
{
    unsigned long StartAddressOfRawData;// Offset=0x0 Size=0x4
    unsigned long EndAddressOfRawData;// Offset=0x4 Size=0x4
    unsigned long AddressOfIndex;// Offset=0x8 Size=0x4
    unsigned long AddressOfCallBacks;// Offset=0xc Size=0x4
    unsigned long SizeOfZeroFill;// Offset=0x10 Size=0x4
    unsigned long Characteristics;// Offset=0x14 Size=0x4
};

struct _NT_TIB// Size=0x1c (Id=910)
{
    struct _EXCEPTION_REGISTRATION_RECORD * ExceptionList;// Offset=0x0 Size=0x4
    void * StackBase;// Offset=0x4 Size=0x4
    void * StackLimit;// Offset=0x8 Size=0x4
    void * SubSystemTib;// Offset=0xc Size=0x4
    union // Size=0x4 (Id=0)
    {
        void * FiberData;// Offset=0x10 Size=0x4
        unsigned long Version;// Offset=0x10 Size=0x4
    };
    void * ArbitraryUserPointer;// Offset=0x14 Size=0x4
    struct _NT_TIB * Self;// Offset=0x18 Size=0x4
};

struct _SYSTEM_POOL_ENTRY// Size=0xc (Id=911)
{
    unsigned char Allocated;// Offset=0x0 Size=0x1
    unsigned char Spare0;// Offset=0x1 Size=0x1
    unsigned short AllocatorBackTraceIndex;// Offset=0x2 Size=0x2
    unsigned long Size;// Offset=0x4 Size=0x4
    union // Size=0xc (Id=0)
    {
        unsigned char Tag[4];// Offset=0x8 Size=0x4
        unsigned long TagUlong;// Offset=0x8 Size=0x4
        void * ProcessChargedQuota;// Offset=0x8 Size=0x4
    };
};

struct _KEY_NODE_INFORMATION// Size=0x20 (Id=918)
{
    union _LARGE_INTEGER LastWriteTime;// Offset=0x0 Size=0x8
    unsigned long TitleIndex;// Offset=0x8 Size=0x4
    unsigned long ClassOffset;// Offset=0xc Size=0x4
    unsigned long ClassLength;// Offset=0x10 Size=0x4
    unsigned long NameLength;// Offset=0x14 Size=0x4
    unsigned short Name[1];// Offset=0x18 Size=0x2
};

struct _CM_PARTIAL_RESOURCE_LIST// Size=0x18 (Id=919)
{
    unsigned short Version;// Offset=0x0 Size=0x2
    unsigned short Revision;// Offset=0x2 Size=0x2
    unsigned long Count;// Offset=0x4 Size=0x4
    struct _CM_PARTIAL_RESOURCE_DESCRIPTOR PartialDescriptors[1];// Offset=0x8 Size=0x10
};

struct _KDEVICE_QUEUE_ENTRY// Size=0x10 (Id=920)
{
    struct _LIST_ENTRY DeviceListEntry;// Offset=0x0 Size=0x8
    unsigned long SortKey;// Offset=0x8 Size=0x4
    unsigned char Inserted;// Offset=0xc Size=0x1
};

struct _RTL_HEAP_TAG_INFO// Size=0xc (Id=922)
{
    unsigned long NumberOfAllocations;// Offset=0x0 Size=0x4
    unsigned long NumberOfFrees;// Offset=0x4 Size=0x4
    unsigned long BytesAllocated;// Offset=0x8 Size=0x4
};

struct _FILE_NOTIFY_INFORMATION// Size=0x10 (Id=923)
{
    unsigned long NextEntryOffset;// Offset=0x0 Size=0x4
    unsigned long Action;// Offset=0x4 Size=0x4
    unsigned long FileNameLength;// Offset=0x8 Size=0x4
    char FileName[1];// Offset=0xc Size=0x1
};

struct _FILE_FS_OBJECTID_INFORMATION// Size=0x40 (Id=931)
{
    unsigned char ObjectId[16];// Offset=0x0 Size=0x10
    unsigned char ExtendedInfo[48];// Offset=0x10 Size=0x30
};

struct _RTL_PROCESS_MODULE_INFORMATION// Size=0x11c (Id=936)
{
    void * Section;// Offset=0x0 Size=0x4
    void * MappedBase;// Offset=0x4 Size=0x4
    void * ImageBase;// Offset=0x8 Size=0x4
    unsigned long ImageSize;// Offset=0xc Size=0x4
    unsigned long Flags;// Offset=0x10 Size=0x4
    unsigned short LoadOrderIndex;// Offset=0x14 Size=0x2
    unsigned short InitOrderIndex;// Offset=0x16 Size=0x2
    unsigned short LoadCount;// Offset=0x18 Size=0x2
    unsigned short OffsetToFileName;// Offset=0x1a Size=0x2
    unsigned char FullPathName[256];// Offset=0x1c Size=0x100
};

struct _IMAGE_ROM_OPTIONAL_HEADER// Size=0x38 (Id=945)
{
    unsigned short Magic;// Offset=0x0 Size=0x2
    unsigned char MajorLinkerVersion;// Offset=0x2 Size=0x1
    unsigned char MinorLinkerVersion;// Offset=0x3 Size=0x1
    unsigned long SizeOfCode;// Offset=0x4 Size=0x4
    unsigned long SizeOfInitializedData;// Offset=0x8 Size=0x4
    unsigned long SizeOfUninitializedData;// Offset=0xc Size=0x4
    unsigned long AddressOfEntryPoint;// Offset=0x10 Size=0x4
    unsigned long BaseOfCode;// Offset=0x14 Size=0x4
    unsigned long BaseOfData;// Offset=0x18 Size=0x4
    unsigned long BaseOfBss;// Offset=0x1c Size=0x4
    unsigned long GprMask;// Offset=0x20 Size=0x4
    unsigned long CprMask[4];// Offset=0x24 Size=0x10
    unsigned long GpValue;// Offset=0x34 Size=0x4
};

struct _ACL_SIZE_INFORMATION// Size=0xc (Id=946)
{
    unsigned long AceCount;// Offset=0x0 Size=0x4
    unsigned long AclBytesInUse;// Offset=0x4 Size=0x4
    unsigned long AclBytesFree;// Offset=0x8 Size=0x4
};

struct _TIMER_BASIC_INFORMATION// Size=0x10 (Id=947)
{
    union _LARGE_INTEGER RemainingTime;// Offset=0x0 Size=0x8
    unsigned char TimerState;// Offset=0x8 Size=0x1
};

struct _IMAGE_BOUND_IMPORT_DESCRIPTOR// Size=0x8 (Id=950)
{
    unsigned long TimeDateStamp;// Offset=0x0 Size=0x4
    unsigned short OffsetModuleName;// Offset=0x4 Size=0x2
    unsigned short NumberOfModuleForwarderRefs;// Offset=0x6 Size=0x2
};

struct _FILE_STREAM_INFORMATION// Size=0x20 (Id=952)
{
    unsigned long NextEntryOffset;// Offset=0x0 Size=0x4
    unsigned long StreamNameLength;// Offset=0x4 Size=0x4
    union _LARGE_INTEGER StreamSize;// Offset=0x8 Size=0x8
    union _LARGE_INTEGER StreamAllocationSize;// Offset=0x10 Size=0x8
    char StreamName[1];// Offset=0x18 Size=0x1
};

struct _IMAGE_DATA_DIRECTORY// Size=0x8 (Id=964)
{
    unsigned long VirtualAddress;// Offset=0x0 Size=0x4
    unsigned long Size;// Offset=0x4 Size=0x4
};

struct _SECURITY_SEED_AND_LENGTH// Size=0x2 (Id=969)
{
    unsigned char Length;// Offset=0x0 Size=0x1
    unsigned char Seed;// Offset=0x1 Size=0x1
};

struct _RTL_HEAP_TAG// Size=0x40 (Id=970)
{
    unsigned long NumberOfAllocations;// Offset=0x0 Size=0x4
    unsigned long NumberOfFrees;// Offset=0x4 Size=0x4
    unsigned long BytesAllocated;// Offset=0x8 Size=0x4
    unsigned short TagIndex;// Offset=0xc Size=0x2
    unsigned short CreatorBackTraceIndex;// Offset=0xe Size=0x2
    unsigned short TagName[24];// Offset=0x10 Size=0x30
};

struct _IO_STATUS_BLOCK// Size=0x8 (Id=972)
{
    union // Size=0x4 (Id=0)
    {
        long Status;// Offset=0x0 Size=0x4
        void * Pointer;// Offset=0x0 Size=0x4
    };
    unsigned long Information;// Offset=0x4 Size=0x4
};

struct _FILE_INTERNAL_INFORMATION// Size=0x8 (Id=999)
{
    union _LARGE_INTEGER IndexNumber;// Offset=0x0 Size=0x8
};

struct _IO_COUNTERS// Size=0x30 (Id=1001)
{
    unsigned long long ReadOperationCount;// Offset=0x0 Size=0x8
    unsigned long long WriteOperationCount;// Offset=0x8 Size=0x8
    unsigned long long OtherOperationCount;// Offset=0x10 Size=0x8
    unsigned long long ReadTransferCount;// Offset=0x18 Size=0x8
    unsigned long long WriteTransferCount;// Offset=0x20 Size=0x8
    unsigned long long OtherTransferCount;// Offset=0x28 Size=0x8
};

struct _FILE_DISPOSITION_INFORMATION// Size=0x1 (Id=1004)
{
    unsigned char DeleteFile;// Offset=0x0 Size=0x1
};

struct _FILE_PIPE_INFORMATION// Size=0x8 (Id=1013)
{
    unsigned long ReadMode;// Offset=0x0 Size=0x4
    unsigned long CompletionMode;// Offset=0x4 Size=0x4
};

struct _IMAGE_IMPORT_BY_NAME// Size=0x4 (Id=1016)
{
    unsigned short Hint;// Offset=0x0 Size=0x2
    unsigned char Name[1];// Offset=0x2 Size=0x1
};

struct _CM_KEYBOARD_DEVICE_DATA// Size=0x8 (Id=1018)
{
    unsigned short Version;// Offset=0x0 Size=0x2
    unsigned short Revision;// Offset=0x2 Size=0x2
    unsigned char Type;// Offset=0x4 Size=0x1
    unsigned char Subtype;// Offset=0x5 Size=0x1
    unsigned short KeyboardFlags;// Offset=0x6 Size=0x2
};

struct _IMAGE_BASE_RELOCATION// Size=0x8 (Id=1020)
{
    unsigned long VirtualAddress;// Offset=0x0 Size=0x4
    unsigned long SizeOfBlock;// Offset=0x4 Size=0x4
};

struct _CM_SONIC_DEVICE_DATA// Size=0xe (Id=1025)
{
    unsigned short Version;// Offset=0x0 Size=0x2
    unsigned short Revision;// Offset=0x2 Size=0x2
    unsigned short DataConfigurationRegister;// Offset=0x4 Size=0x2
    unsigned char EthernetAddress[8];// Offset=0x6 Size=0x8
};

struct _IO_RESOURCE_REQUIREMENTS_LIST// Size=0x48 (Id=1031)
{
    unsigned long ListSize;// Offset=0x0 Size=0x4
    enum _INTERFACE_TYPE InterfaceType;// Offset=0x4 Size=0x4
    unsigned long BusNumber;// Offset=0x8 Size=0x4
    unsigned long SlotNumber;// Offset=0xc Size=0x4
    unsigned long Reserved[3];// Offset=0x10 Size=0xc
    unsigned long AlternativeLists;// Offset=0x1c Size=0x4
    struct _IO_RESOURCE_LIST List[1];// Offset=0x20 Size=0x28
};

struct _SYSTEM_MEMORY_INFORMATION// Size=0x14 (Id=1043)
{
    unsigned long InfoSize;// Offset=0x0 Size=0x4
    unsigned long StringStart;// Offset=0x4 Size=0x4
    struct _SYSTEM_MEMORY_INFO Memory[1];// Offset=0x8 Size=0xc
};

struct _TOKEN_GROUPS// Size=0xc (Id=1045)
{
    unsigned long GroupCount;// Offset=0x0 Size=0x4
    struct _SID_AND_ATTRIBUTES Groups[1];// Offset=0x4 Size=0x8
};

struct _IMAGE_THUNK_DATA64// Size=0x8 (Id=1055)
{
    union __unnamed u1;// Offset=0x0 Size=0x8
};

struct _KWAIT_BLOCK// Size=0x18 (Id=1057)
{
    struct _LIST_ENTRY WaitListEntry;// Offset=0x0 Size=0x8
    struct _KTHREAD * Thread;// Offset=0x8 Size=0x4
    void * Object;// Offset=0xc Size=0x4
    struct _KWAIT_BLOCK * NextWaitBlock;// Offset=0x10 Size=0x4
    unsigned short WaitKey;// Offset=0x14 Size=0x2
    unsigned short WaitType;// Offset=0x16 Size=0x2
};

struct _IMAGE_RESOURCE_DIRECTORY// Size=0x10 (Id=1062)
{
    unsigned long Characteristics;// Offset=0x0 Size=0x4
    unsigned long TimeDateStamp;// Offset=0x4 Size=0x4
    unsigned short MajorVersion;// Offset=0x8 Size=0x2
    unsigned short MinorVersion;// Offset=0xa Size=0x2
    unsigned short NumberOfNamedEntries;// Offset=0xc Size=0x2
    unsigned short NumberOfIdEntries;// Offset=0xe Size=0x2
};

struct _RTL_HANDLE_TABLE_ENTRY// Size=0x4 (Id=1074)
{
    union // Size=0x4 (Id=0)
    {
        unsigned long Flags;// Offset=0x0 Size=0x4
        struct _RTL_HANDLE_TABLE_ENTRY * NextFree;// Offset=0x0 Size=0x4
    };
};

struct _IMAGE_TLS_DIRECTORY64// Size=0x28 (Id=1075)
{
    unsigned long long StartAddressOfRawData;// Offset=0x0 Size=0x8
    unsigned long long EndAddressOfRawData;// Offset=0x8 Size=0x8
    unsigned long long AddressOfIndex;// Offset=0x10 Size=0x8
    unsigned long long AddressOfCallBacks;// Offset=0x18 Size=0x8
    unsigned long SizeOfZeroFill;// Offset=0x20 Size=0x4
    unsigned long Characteristics;// Offset=0x24 Size=0x4
};

struct _SECURITY_ADVANCED_QUALITY_OF_SERVICE// Size=0x14 (Id=1076)
{
    unsigned long Length;// Offset=0x0 Size=0x4
    enum _SECURITY_IMPERSONATION_LEVEL ImpersonationLevel;// Offset=0x4 Size=0x4
    unsigned char ContextTrackingMode;// Offset=0x8 Size=0x1
    unsigned char EffectiveOnly;// Offset=0x9 Size=0x1
    unsigned char __align0[2];// Offset=0xa Size=0x2
    struct _SECURITY_TOKEN_PROXY_DATA * ProxyData;// Offset=0xc Size=0x4
    struct _SECURITY_TOKEN_AUDIT_DATA * AuditData;// Offset=0x10 Size=0x4
};

struct _SYSTEM_TIMEOFDAY_INFORMATION// Size=0x30 (Id=1084)
{
    union _LARGE_INTEGER BootTime;// Offset=0x0 Size=0x8
    union _LARGE_INTEGER CurrentTime;// Offset=0x8 Size=0x8
    union _LARGE_INTEGER TimeZoneBias;// Offset=0x10 Size=0x8
    unsigned long TimeZoneId;// Offset=0x18 Size=0x4
    unsigned long Reserved;// Offset=0x1c Size=0x4
    unsigned long long BootTimeBias;// Offset=0x20 Size=0x8
    unsigned long long SleepTimeBias;// Offset=0x28 Size=0x8
};

struct _CM_EISA_FUNCTION_INFORMATION// Size=0x140 (Id=1085)
{
    unsigned long CompressedId;// Offset=0x0 Size=0x4
    unsigned char IdSlotFlags1;// Offset=0x4 Size=0x1
    unsigned char IdSlotFlags2;// Offset=0x5 Size=0x1
    unsigned char MinorRevision;// Offset=0x6 Size=0x1
    unsigned char MajorRevision;// Offset=0x7 Size=0x1
    unsigned char Selections[26];// Offset=0x8 Size=0x1a
    unsigned char FunctionFlags;// Offset=0x22 Size=0x1
    unsigned char TypeString[80];// Offset=0x23 Size=0x50
    struct _EISA_MEMORY_CONFIGURATION EisaMemory[9];// Offset=0x73 Size=0x3f
    struct _EISA_IRQ_CONFIGURATION EisaIrq[7];// Offset=0xb2 Size=0xe
    struct _EISA_DMA_CONFIGURATION EisaDma[4];// Offset=0xc0 Size=0x8
    struct _EISA_PORT_CONFIGURATION EisaPort[20];// Offset=0xc8 Size=0x3c
    unsigned char InitializationData[60];// Offset=0x104 Size=0x3c
};

struct _ACL_REVISION_INFORMATION// Size=0x4 (Id=1087)
{
    unsigned long AclRevision;// Offset=0x0 Size=0x4
};

struct _IMAGE_IMPORT_DESCRIPTOR// Size=0x14 (Id=1088)
{
    union // Size=0x4 (Id=0)
    {
        unsigned long Characteristics;// Offset=0x0 Size=0x4
        unsigned long OriginalFirstThunk;// Offset=0x0 Size=0x4
    };
    unsigned long TimeDateStamp;// Offset=0x4 Size=0x4
    unsigned long ForwarderChain;// Offset=0x8 Size=0x4
    unsigned long Name;// Offset=0xc Size=0x4
    unsigned long FirstThunk;// Offset=0x10 Size=0x4
};

struct _CM_VIDEO_DEVICE_DATA// Size=0x8 (Id=1098)
{
    unsigned short Version;// Offset=0x0 Size=0x2
    unsigned short Revision;// Offset=0x2 Size=0x2
    unsigned long VideoClock;// Offset=0x4 Size=0x4
};

struct _PROCESS_HEAP_ENTRY::__unnamed::__unnamed// Size=0x10 (Id=1117)
{
    void * hMem;// Offset=0x0 Size=0x4
    unsigned long dwReserved[3];// Offset=0x4 Size=0xc
};

struct _PROCESS_HEAP_ENTRY::__unnamed::__unnamed// Size=0x10 (Id=1118)
{
    unsigned long dwCommittedSize;// Offset=0x0 Size=0x4
    unsigned long dwUnCommittedSize;// Offset=0x4 Size=0x4
    void * lpFirstBlock;// Offset=0x8 Size=0x4
    void * lpLastBlock;// Offset=0xc Size=0x4
};

struct _PROCESS_HEAP_ENTRY// Size=0x1c (Id=1119)
{
    void * lpData;// Offset=0x0 Size=0x4
    unsigned long cbData;// Offset=0x4 Size=0x4
    unsigned char cbOverhead;// Offset=0x8 Size=0x1
    unsigned char iRegionIndex;// Offset=0x9 Size=0x1
    unsigned short wFlags;// Offset=0xa Size=0x2
    union // Size=0x10 (Id=0)
    {
        struct _PROCESS_HEAP_ENTRY::__unnamed::__unnamed Block;// Offset=0xc Size=0x10
        struct _PROCESS_HEAP_ENTRY::__unnamed::__unnamed Region;// Offset=0xc Size=0x10
    };
};

struct _IMAGE_AUX_SYMBOL::__unnamed::__unnamed::__unnamed// Size=0x4 (Id=1142)
{
    unsigned short Linenumber;// Offset=0x0 Size=0x2
    unsigned short Size;// Offset=0x2 Size=0x2
};

union _IMAGE_AUX_SYMBOL::__unnamed::__unnamed// Size=0x4 (Id=1143)
{
    struct _IMAGE_AUX_SYMBOL::__unnamed::__unnamed::__unnamed LnSz;// Offset=0x0 Size=0x4
    unsigned long TotalSize;// Offset=0x0 Size=0x4
};

struct _IMAGE_AUX_SYMBOL::__unnamed::__unnamed::__unnamed// Size=0x8 (Id=1144)
{
    unsigned long PointerToLinenumber;// Offset=0x0 Size=0x4
    unsigned long PointerToNextFunction;// Offset=0x4 Size=0x4
};

struct _IMAGE_AUX_SYMBOL::__unnamed::__unnamed::__unnamed// Size=0x8 (Id=1145)
{
    unsigned short Dimension[4];// Offset=0x0 Size=0x8
};

union _IMAGE_AUX_SYMBOL::__unnamed::__unnamed// Size=0x8 (Id=1146)
{
    struct _IMAGE_AUX_SYMBOL::__unnamed::__unnamed::__unnamed Function;// Offset=0x0 Size=0x8
    struct _IMAGE_AUX_SYMBOL::__unnamed::__unnamed::__unnamed Array;// Offset=0x0 Size=0x8
};

struct _IMAGE_AUX_SYMBOL::__unnamed// Size=0x12 (Id=1147)
{
    unsigned long TagIndex;// Offset=0x0 Size=0x4
    union _IMAGE_AUX_SYMBOL::__unnamed::__unnamed Misc;// Offset=0x4 Size=0x4
    union _IMAGE_AUX_SYMBOL::__unnamed::__unnamed FcnAry;// Offset=0x8 Size=0x8
    unsigned short TvIndex;// Offset=0x10 Size=0x2
};

struct _IMAGE_AUX_SYMBOL::__unnamed// Size=0x12 (Id=1148)
{
    unsigned char Name[18];// Offset=0x0 Size=0x12
};

struct _IMAGE_AUX_SYMBOL::__unnamed// Size=0x10 (Id=1149)
{
    unsigned long Length;// Offset=0x0 Size=0x4
    unsigned short NumberOfRelocations;// Offset=0x4 Size=0x2
    unsigned short NumberOfLinenumbers;// Offset=0x6 Size=0x2
    unsigned long CheckSum;// Offset=0x8 Size=0x4
    short Number;// Offset=0xc Size=0x2
    unsigned char Selection;// Offset=0xe Size=0x1
};

union _IMAGE_AUX_SYMBOL// Size=0x12 (Id=1150)
{
    union // Size=0x12 (Id=0)
    {
        struct _IMAGE_AUX_SYMBOL::__unnamed Sym;// Offset=0x0 Size=0x12
        struct _IMAGE_AUX_SYMBOL::__unnamed File;// Offset=0x0 Size=0x12
        struct _IMAGE_AUX_SYMBOL::__unnamed Section;// Offset=0x0 Size=0x10
    };
};

union _IMAGE_LINENUMBER::__unnamed// Size=0x4 (Id=1167)
{
    unsigned long SymbolTableIndex;// Offset=0x0 Size=0x4
    unsigned long VirtualAddress;// Offset=0x0 Size=0x4
};

struct _IMAGE_LINENUMBER// Size=0x6 (Id=1168)
{
    union _IMAGE_LINENUMBER::__unnamed Type;// Offset=0x0 Size=0x4
    unsigned short Linenumber;// Offset=0x4 Size=0x2
};

struct _IMAGE_SYMBOL::__unnamed::__unnamed// Size=0x8 (Id=1208)
{
    unsigned long Short;// Offset=0x0 Size=0x4
    unsigned long Long;// Offset=0x4 Size=0x4
};

union _IMAGE_SYMBOL::__unnamed// Size=0x8 (Id=1209)
{
    unsigned char ShortName[8];// Offset=0x0 Size=0x8
    struct _IMAGE_SYMBOL::__unnamed::__unnamed Name;// Offset=0x0 Size=0x8
    unsigned long LongName[2];// Offset=0x0 Size=0x8
};

struct _IMAGE_SYMBOL// Size=0x12 (Id=1210)
{
    union _IMAGE_SYMBOL::__unnamed N;// Offset=0x0 Size=0x8
    unsigned long Value;// Offset=0x8 Size=0x4
    short SectionNumber;// Offset=0xc Size=0x2
    unsigned short Type;// Offset=0xe Size=0x2
    unsigned char StorageClass;// Offset=0x10 Size=0x1
    unsigned char NumberOfAuxSymbols;// Offset=0x11 Size=0x1
};

union _IMAGE_THUNK_DATA32::__unnamed// Size=0x4 (Id=1213)
{
    unsigned long ForwarderString;// Offset=0x0 Size=0x4
    unsigned long Function;// Offset=0x0 Size=0x4
    unsigned long Ordinal;// Offset=0x0 Size=0x4
    unsigned long AddressOfData;// Offset=0x0 Size=0x4
};

struct _IMAGE_THUNK_DATA32// Size=0x4 (Id=1214)
{
    union _IMAGE_THUNK_DATA32::__unnamed u1;// Offset=0x0 Size=0x4
};

union _RTL_CRITICAL_SECTION::__unnamed// Size=0x10 (Id=1276)
{
    unsigned long RawEvent[4];// Offset=0x0 Size=0x10
};

struct _RTL_CRITICAL_SECTION// Size=0x1c (Id=1277)
{
    union _RTL_CRITICAL_SECTION::__unnamed Synchronization;// Offset=0x0 Size=0x10
    long LockCount;// Offset=0x10 Size=0x4
    long RecursionCount;// Offset=0x14 Size=0x4
    void * OwningThread;// Offset=0x18 Size=0x4
};

union _IMAGE_SECTION_HEADER::__unnamed// Size=0x4 (Id=1296)
{
    unsigned long PhysicalAddress;// Offset=0x0 Size=0x4
    unsigned long VirtualSize;// Offset=0x0 Size=0x4
};

struct _IMAGE_SECTION_HEADER// Size=0x28 (Id=1297)
{
    unsigned char Name[8];// Offset=0x0 Size=0x8
    union _IMAGE_SECTION_HEADER::__unnamed Misc;// Offset=0x8 Size=0x4
    unsigned long VirtualAddress;// Offset=0xc Size=0x4
    unsigned long SizeOfRawData;// Offset=0x10 Size=0x4
    unsigned long PointerToRawData;// Offset=0x14 Size=0x4
    unsigned long PointerToRelocations;// Offset=0x18 Size=0x4
    unsigned long PointerToLinenumbers;// Offset=0x1c Size=0x4
    unsigned short NumberOfRelocations;// Offset=0x20 Size=0x2
    unsigned short NumberOfLinenumbers;// Offset=0x22 Size=0x2
    unsigned long Characteristics;// Offset=0x24 Size=0x4
};

union _IMAGE_THUNK_DATA64::__unnamed// Size=0x8 (Id=1370)
{
    unsigned long long ForwarderString;// Offset=0x0 Size=0x8
    unsigned long long Function;// Offset=0x0 Size=0x8
    unsigned long long Ordinal;// Offset=0x0 Size=0x8
    unsigned long long AddressOfData;// Offset=0x0 Size=0x8
};

struct _IMAGE_THUNK_DATA64// Size=0x8 (Id=1371)
{
    union _IMAGE_THUNK_DATA64::__unnamed u1;// Offset=0x0 Size=0x8
};

struct _IO_RESOURCE_DESCRIPTOR::__unnamed::__unnamed// Size=0x18 (Id=1427)
{
    unsigned long Length;// Offset=0x0 Size=0x4
    unsigned long Alignment;// Offset=0x4 Size=0x4
    union _LARGE_INTEGER MinimumAddress;// Offset=0x8 Size=0x8
    union _LARGE_INTEGER MaximumAddress;// Offset=0x10 Size=0x8
};

struct _IO_RESOURCE_DESCRIPTOR::__unnamed::__unnamed// Size=0x8 (Id=1428)
{
    unsigned long MinimumVector;// Offset=0x0 Size=0x4
    unsigned long MaximumVector;// Offset=0x4 Size=0x4
};

struct _IO_RESOURCE_DESCRIPTOR::__unnamed::__unnamed// Size=0x8 (Id=1429)
{
    unsigned long MinimumChannel;// Offset=0x0 Size=0x4
    unsigned long MaximumChannel;// Offset=0x4 Size=0x4
};

struct _IO_RESOURCE_DESCRIPTOR::__unnamed::__unnamed// Size=0xc (Id=1430)
{
    unsigned long Data[3];// Offset=0x0 Size=0xc
};

struct _IO_RESOURCE_DESCRIPTOR::__unnamed::__unnamed// Size=0x10 (Id=1431)
{
    unsigned long Length;// Offset=0x0 Size=0x4
    unsigned long MinBusNumber;// Offset=0x4 Size=0x4
    unsigned long MaxBusNumber;// Offset=0x8 Size=0x4
    unsigned long Reserved;// Offset=0xc Size=0x4
};

struct _IO_RESOURCE_DESCRIPTOR::__unnamed::__unnamed// Size=0x4 (Id=1432)
{
    void * AssignedResource;// Offset=0x0 Size=0x4
};

struct _IO_RESOURCE_DESCRIPTOR::__unnamed::__unnamed// Size=0x10 (Id=1433)
{
    unsigned char Type;// Offset=0x0 Size=0x1
    unsigned char Reserved[3];// Offset=0x1 Size=0x3
    void * AssignedResource;// Offset=0x4 Size=0x4
    union _LARGE_INTEGER Transformation;// Offset=0x8 Size=0x8
};

struct _IO_RESOURCE_DESCRIPTOR::__unnamed::__unnamed// Size=0xc (Id=1434)
{
    unsigned long Priority;// Offset=0x0 Size=0x4
    unsigned long Reserved1;// Offset=0x4 Size=0x4
    unsigned long Reserved2;// Offset=0x8 Size=0x4
};

union _IO_RESOURCE_DESCRIPTOR::__unnamed// Size=0x18 (Id=1435)
{
    union // Size=0x18 (Id=0)
    {
        struct _IO_RESOURCE_DESCRIPTOR::__unnamed::__unnamed Port;// Offset=0x0 Size=0x18
        struct _IO_RESOURCE_DESCRIPTOR::__unnamed::__unnamed Memory;// Offset=0x0 Size=0x18
        struct _IO_RESOURCE_DESCRIPTOR::__unnamed::__unnamed Interrupt;// Offset=0x0 Size=0x8
        struct _IO_RESOURCE_DESCRIPTOR::__unnamed::__unnamed Dma;// Offset=0x0 Size=0x8
        struct _IO_RESOURCE_DESCRIPTOR::__unnamed::__unnamed Generic;// Offset=0x0 Size=0x18
        struct _IO_RESOURCE_DESCRIPTOR::__unnamed::__unnamed DevicePrivate;// Offset=0x0 Size=0xc
        struct _IO_RESOURCE_DESCRIPTOR::__unnamed::__unnamed BusNumber;// Offset=0x0 Size=0x10
        struct _IO_RESOURCE_DESCRIPTOR::__unnamed::__unnamed AssignedResource;// Offset=0x0 Size=0x4
        struct _IO_RESOURCE_DESCRIPTOR::__unnamed::__unnamed SubAllocateFrom;// Offset=0x0 Size=0x10
        struct _IO_RESOURCE_DESCRIPTOR::__unnamed::__unnamed ConfigData;// Offset=0x0 Size=0xc
    };
};

struct _IO_RESOURCE_DESCRIPTOR// Size=0x20 (Id=1436)
{
    unsigned char Option;// Offset=0x0 Size=0x1
    unsigned char Type;// Offset=0x1 Size=0x1
    unsigned char ShareDisposition;// Offset=0x2 Size=0x1
    unsigned char Spare1;// Offset=0x3 Size=0x1
    unsigned short Flags;// Offset=0x4 Size=0x2
    unsigned short Spare2;// Offset=0x6 Size=0x2
    union _IO_RESOURCE_DESCRIPTOR::__unnamed u;// Offset=0x8 Size=0x18
};

struct _RTL_HEAP_ENTRY::__unnamed::__unnamed// Size=0x8 (Id=1444)
{
    unsigned long Settable;// Offset=0x0 Size=0x4
    unsigned long Tag;// Offset=0x4 Size=0x4
};

struct _RTL_HEAP_ENTRY::__unnamed::__unnamed// Size=0x8 (Id=1445)
{
    unsigned long CommittedSize;// Offset=0x0 Size=0x4
    void * FirstBlock;// Offset=0x4 Size=0x4
};

union _RTL_HEAP_ENTRY::__unnamed// Size=0x8 (Id=1446)
{
    struct _RTL_HEAP_ENTRY::__unnamed::__unnamed s1;// Offset=0x0 Size=0x8
    struct _RTL_HEAP_ENTRY::__unnamed::__unnamed s2;// Offset=0x0 Size=0x8
};

struct _RTL_HEAP_ENTRY// Size=0x10 (Id=1447)
{
    unsigned long Size;// Offset=0x0 Size=0x4
    unsigned short Flags;// Offset=0x4 Size=0x2
    unsigned short AllocatorBackTraceIndex;// Offset=0x6 Size=0x2
    union _RTL_HEAP_ENTRY::__unnamed u;// Offset=0x8 Size=0x8
};

struct _KTHREAD// Size=0x110 (Id=1507)
{
    struct _DISPATCHER_HEADER Header;// Offset=0x0 Size=0x10
    struct _LIST_ENTRY MutantListHead;// Offset=0x10 Size=0x8
    unsigned long KernelTime;// Offset=0x18 Size=0x4
    void * StackBase;// Offset=0x1c Size=0x4
    void * StackLimit;// Offset=0x20 Size=0x4
    void * KernelStack;// Offset=0x24 Size=0x4
    void * TlsData;// Offset=0x28 Size=0x4
    unsigned char State;// Offset=0x2c Size=0x1
    unsigned char Alerted[2];// Offset=0x2d Size=0x2
    unsigned char Alertable;// Offset=0x2f Size=0x1
    unsigned char NpxState;// Offset=0x30 Size=0x1
    char Saturation;// Offset=0x31 Size=0x1
    char Priority;// Offset=0x32 Size=0x1
    unsigned char Padding;// Offset=0x33 Size=0x1
    struct _KAPC_STATE ApcState;// Offset=0x34 Size=0x18
    unsigned long ContextSwitches;// Offset=0x4c Size=0x4
    long WaitStatus;// Offset=0x50 Size=0x4
    unsigned char WaitIrql;// Offset=0x54 Size=0x1
    char WaitMode;// Offset=0x55 Size=0x1
    unsigned char WaitNext;// Offset=0x56 Size=0x1
    unsigned char WaitReason;// Offset=0x57 Size=0x1
    struct _KWAIT_BLOCK * WaitBlockList;// Offset=0x58 Size=0x4
    struct _LIST_ENTRY WaitListEntry;// Offset=0x5c Size=0x8
    unsigned long WaitTime;// Offset=0x64 Size=0x4
    unsigned long KernelApcDisable;// Offset=0x68 Size=0x4
    long Quantum;// Offset=0x6c Size=0x4
    char BasePriority;// Offset=0x70 Size=0x1
    unsigned char DecrementCount;// Offset=0x71 Size=0x1
    char PriorityDecrement;// Offset=0x72 Size=0x1
    unsigned char DisableBoost;// Offset=0x73 Size=0x1
    unsigned char NpxIrql;// Offset=0x74 Size=0x1
    char SuspendCount;// Offset=0x75 Size=0x1
    unsigned char Preempted;// Offset=0x76 Size=0x1
    unsigned char HasTerminated;// Offset=0x77 Size=0x1
    struct _KQUEUE * Queue;// Offset=0x78 Size=0x4
    struct _LIST_ENTRY QueueListEntry;// Offset=0x7c Size=0x8
    unsigned char __align0[4];// Offset=0x84 Size=0x4
    struct _KTIMER Timer;// Offset=0x88 Size=0x28
    struct _KWAIT_BLOCK TimerWaitBlock;// Offset=0xb0 Size=0x18
    struct _KAPC SuspendApc;// Offset=0xc8 Size=0x28
    struct _KSEMAPHORE SuspendSemaphore;// Offset=0xf0 Size=0x14
    struct _LIST_ENTRY ThreadListEntry;// Offset=0x104 Size=0x8
};

struct _KPROCESS// Size=0x1c (Id=1583)
{
    struct _LIST_ENTRY ReadyListHead;// Offset=0x0 Size=0x8
    struct _LIST_ENTRY ThreadListHead;// Offset=0x8 Size=0x8
    unsigned long StackCount;// Offset=0x10 Size=0x4
    long ThreadQuantum;// Offset=0x14 Size=0x4
    char BasePriority;// Offset=0x18 Size=0x1
    unsigned char DisableBoost;// Offset=0x19 Size=0x1
    unsigned char DisableQuantum;// Offset=0x1a Size=0x1
};

struct _RTL_CRITICAL_SECTION::__unnamed::__unnamed// Size=0x10 (Id=1589)
{
    unsigned char Type;// Offset=0x0 Size=0x1
    unsigned char Absolute;// Offset=0x1 Size=0x1
    unsigned char Size;// Offset=0x2 Size=0x1
    unsigned char Inserted;// Offset=0x3 Size=0x1
    long SignalState;// Offset=0x4 Size=0x4
    struct _LIST_ENTRY WaitListHead;// Offset=0x8 Size=0x8
};

union _RTL_CRITICAL_SECTION::__unnamed// Size=0x10 (Id=1590)
{
    struct _RTL_CRITICAL_SECTION::__unnamed::__unnamed Event;// Offset=0x0 Size=0x10
    unsigned long RawEvent[4];// Offset=0x0 Size=0x10
};

struct _RTL_CRITICAL_SECTION// Size=0x1c (Id=1591)
{
    union _RTL_CRITICAL_SECTION::__unnamed Synchronization;// Offset=0x0 Size=0x10
    long LockCount;// Offset=0x10 Size=0x4
    long RecursionCount;// Offset=0x14 Size=0x4
    void * OwningThread;// Offset=0x18 Size=0x4
};

struct _RTL_HEAP_WALK_ENTRY::__unnamed::__unnamed// Size=0x10 (Id=1592)
{
    unsigned long Settable;// Offset=0x0 Size=0x4
    unsigned short TagIndex;// Offset=0x4 Size=0x2
    unsigned short AllocatorBackTraceIndex;// Offset=0x6 Size=0x2
    unsigned long Reserved[2];// Offset=0x8 Size=0x8
};

struct _RTL_HEAP_WALK_ENTRY::__unnamed::__unnamed// Size=0x10 (Id=1593)
{
    unsigned long CommittedSize;// Offset=0x0 Size=0x4
    unsigned long UnCommittedSize;// Offset=0x4 Size=0x4
    void * FirstEntry;// Offset=0x8 Size=0x4
    void * LastEntry;// Offset=0xc Size=0x4
};

struct _RTL_HEAP_WALK_ENTRY// Size=0x1c (Id=1594)
{
    void * DataAddress;// Offset=0x0 Size=0x4
    unsigned long DataSize;// Offset=0x4 Size=0x4
    unsigned char OverheadBytes;// Offset=0x8 Size=0x1
    unsigned char SegmentIndex;// Offset=0x9 Size=0x1
    unsigned short Flags;// Offset=0xa Size=0x2
    union // Size=0x10 (Id=0)
    {
        struct _RTL_HEAP_WALK_ENTRY::__unnamed::__unnamed Block;// Offset=0xc Size=0x10
        struct _RTL_HEAP_WALK_ENTRY::__unnamed::__unnamed Segment;// Offset=0xc Size=0x10
    };
};

struct _CM_PARTIAL_RESOURCE_DESCRIPTOR::__unnamed::__unnamed// Size=0xc (Id=1660)
{
    union _LARGE_INTEGER Start;// Offset=0x0 Size=0x8
    unsigned long Length;// Offset=0x8 Size=0x4
};

struct _CM_PARTIAL_RESOURCE_DESCRIPTOR::__unnamed::__unnamed// Size=0xc (Id=1661)
{
    unsigned long Level;// Offset=0x0 Size=0x4
    unsigned long Vector;// Offset=0x4 Size=0x4
    unsigned long Affinity;// Offset=0x8 Size=0x4
};

struct _CM_PARTIAL_RESOURCE_DESCRIPTOR::__unnamed::__unnamed// Size=0xc (Id=1662)
{
    unsigned long Channel;// Offset=0x0 Size=0x4
    unsigned long Port;// Offset=0x4 Size=0x4
    unsigned long Reserved1;// Offset=0x8 Size=0x4
};

struct _CM_PARTIAL_RESOURCE_DESCRIPTOR::__unnamed::__unnamed// Size=0xc (Id=1663)
{
    unsigned long Data[3];// Offset=0x0 Size=0xc
};

struct _CM_PARTIAL_RESOURCE_DESCRIPTOR::__unnamed::__unnamed// Size=0xc (Id=1664)
{
    unsigned long Start;// Offset=0x0 Size=0x4
    unsigned long Length;// Offset=0x4 Size=0x4
    unsigned long Reserved;// Offset=0x8 Size=0x4
};

struct _CM_PARTIAL_RESOURCE_DESCRIPTOR::__unnamed::__unnamed// Size=0xc (Id=1665)
{
    unsigned long DataSize;// Offset=0x0 Size=0x4
    unsigned long Reserved1;// Offset=0x4 Size=0x4
    unsigned long Reserved2;// Offset=0x8 Size=0x4
};

union _CM_PARTIAL_RESOURCE_DESCRIPTOR::__unnamed// Size=0xc (Id=1666)
{
    struct _CM_PARTIAL_RESOURCE_DESCRIPTOR::__unnamed::__unnamed Generic;// Offset=0x0 Size=0xc
    struct _CM_PARTIAL_RESOURCE_DESCRIPTOR::__unnamed::__unnamed Port;// Offset=0x0 Size=0xc
    struct _CM_PARTIAL_RESOURCE_DESCRIPTOR::__unnamed::__unnamed Interrupt;// Offset=0x0 Size=0xc
    struct _CM_PARTIAL_RESOURCE_DESCRIPTOR::__unnamed::__unnamed Memory;// Offset=0x0 Size=0xc
    struct _CM_PARTIAL_RESOURCE_DESCRIPTOR::__unnamed::__unnamed Dma;// Offset=0x0 Size=0xc
    struct _CM_PARTIAL_RESOURCE_DESCRIPTOR::__unnamed::__unnamed DevicePrivate;// Offset=0x0 Size=0xc
    struct _CM_PARTIAL_RESOURCE_DESCRIPTOR::__unnamed::__unnamed BusNumber;// Offset=0x0 Size=0xc
    struct _CM_PARTIAL_RESOURCE_DESCRIPTOR::__unnamed::__unnamed DeviceSpecificData;// Offset=0x0 Size=0xc
};

struct _CM_PARTIAL_RESOURCE_DESCRIPTOR// Size=0x10 (Id=1667)
{
    unsigned char Type;// Offset=0x0 Size=0x1
    unsigned char ShareDisposition;// Offset=0x1 Size=0x1
    unsigned short Flags;// Offset=0x2 Size=0x2
    union _CM_PARTIAL_RESOURCE_DESCRIPTOR::__unnamed u;// Offset=0x4 Size=0xc
};

struct _KGDTENTRY::__unnamed::__unnamed// Size=0x4 (Id=1669)
{
    unsigned char BaseMid;// Offset=0x0 Size=0x1
    unsigned char Flags1;// Offset=0x1 Size=0x1
    unsigned char Flags2;// Offset=0x2 Size=0x1
    unsigned char BaseHi;// Offset=0x3 Size=0x1
};

struct _KGDTENTRY::__unnamed::__unnamed// Size=0x4 (Id=1670)
{
    struct // Size=0x4 (Id=0)
    {
        unsigned long BaseMid:8;// Offset=0x0 Size=0x4 BitOffset=0x0 BitSize=0x8
        unsigned long Type:5;// Offset=0x0 Size=0x4 BitOffset=0x8 BitSize=0x5
        unsigned long Dpl:2;// Offset=0x0 Size=0x4 BitOffset=0xd BitSize=0x2
        unsigned long Pres:1;// Offset=0x0 Size=0x4 BitOffset=0xf BitSize=0x1
        unsigned long LimitHi:4;// Offset=0x0 Size=0x4 BitOffset=0x10 BitSize=0x4
        unsigned long Sys:1;// Offset=0x0 Size=0x4 BitOffset=0x14 BitSize=0x1
        unsigned long Reserved_0:1;// Offset=0x0 Size=0x4 BitOffset=0x15 BitSize=0x1
        unsigned long Default_Big:1;// Offset=0x0 Size=0x4 BitOffset=0x16 BitSize=0x1
        unsigned long Granularity:1;// Offset=0x0 Size=0x4 BitOffset=0x17 BitSize=0x1
        unsigned long BaseHi:8;// Offset=0x0 Size=0x4 BitOffset=0x18 BitSize=0x8
    };
};

union _KGDTENTRY::__unnamed// Size=0x4 (Id=1671)
{
    struct _KGDTENTRY::__unnamed::__unnamed Bytes;// Offset=0x0 Size=0x4
    struct _KGDTENTRY::__unnamed::__unnamed Bits;// Offset=0x0 Size=0x4
};

struct _KGDTENTRY// Size=0x8 (Id=1672)
{
    unsigned short LimitLow;// Offset=0x0 Size=0x2
    unsigned short BaseLow;// Offset=0x2 Size=0x2
    union _KGDTENTRY::__unnamed HighWord;// Offset=0x4 Size=0x4
};

struct _PROCESS_DEVICEMAP_INFORMATION::__unnamed::__unnamed// Size=0x4 (Id=1685)
{
    void * DirectoryHandle;// Offset=0x0 Size=0x4
};

struct _PROCESS_DEVICEMAP_INFORMATION::__unnamed::__unnamed// Size=0x24 (Id=1686)
{
    unsigned long DriveMap;// Offset=0x0 Size=0x4
    unsigned char DriveType[32];// Offset=0x4 Size=0x20
};

struct _PROCESS_DEVICEMAP_INFORMATION// Size=0x24 (Id=1687)
{
    union // Size=0x4 (Id=0)
    {
        struct _PROCESS_DEVICEMAP_INFORMATION::__unnamed::__unnamed Set;// Offset=0x0 Size=0x4
        struct _PROCESS_DEVICEMAP_INFORMATION::__unnamed::__unnamed Query;// Offset=0x0 Size=0x24
    };
};

struct _PORT_MESSAGE::__unnamed::__unnamed// Size=0x4 (Id=1694)
{
    short DataLength;// Offset=0x0 Size=0x2
    short TotalLength;// Offset=0x2 Size=0x2
};

union _PORT_MESSAGE::__unnamed// Size=0x4 (Id=1695)
{
    struct _PORT_MESSAGE::__unnamed::__unnamed s1;// Offset=0x0 Size=0x4
    unsigned long Length;// Offset=0x0 Size=0x4
};

struct _PORT_MESSAGE::__unnamed::__unnamed// Size=0x4 (Id=1696)
{
    short Type;// Offset=0x0 Size=0x2
    short DataInfoOffset;// Offset=0x2 Size=0x2
};

union _PORT_MESSAGE::__unnamed// Size=0x4 (Id=1697)
{
    struct _PORT_MESSAGE::__unnamed::__unnamed s2;// Offset=0x0 Size=0x4
    unsigned long ZeroInit;// Offset=0x0 Size=0x4
};

struct _PORT_MESSAGE// Size=0x18 (Id=1698)
{
    union _PORT_MESSAGE::__unnamed u1;// Offset=0x0 Size=0x4
    union _PORT_MESSAGE::__unnamed u2;// Offset=0x4 Size=0x4
    union // Size=0x8 (Id=0)
    {
        struct _CLIENT_ID ClientId;// Offset=0x8 Size=0x8
        float DoNotUseThisField;// Offset=0x8 Size=0x8
    };
    unsigned long MessageId;// Offset=0x10 Size=0x4
    union // Size=0x4 (Id=0)
    {
        unsigned long ClientViewSize;// Offset=0x14 Size=0x4
        unsigned long CallbackId;// Offset=0x14 Size=0x4
    };
};

struct _IO_STACK_LOCATION::__unnamed::__unnamed// Size=0x10 (Id=1700)
{
    unsigned long DesiredAccess;// Offset=0x0 Size=0x4
    unsigned long Options;// Offset=0x4 Size=0x4
    unsigned short FileAttributes;// Offset=0x8 Size=0x2
    unsigned short ShareAccess;// Offset=0xa Size=0x2
    struct _STRING * RemainingName;// Offset=0xc Size=0x4
};

struct _IO_STACK_LOCATION::__unnamed::__unnamed// Size=0x10 (Id=1701)
{
    unsigned long Length;// Offset=0x0 Size=0x4
    union // Size=0x4 (Id=0)
    {
        unsigned long BufferOffset;// Offset=0x4 Size=0x4
        void * CacheBuffer;// Offset=0x4 Size=0x4
    };
    union _LARGE_INTEGER ByteOffset;// Offset=0x8 Size=0x8
};

struct _IO_STACK_LOCATION::__unnamed::__unnamed// Size=0xc (Id=1702)
{
    unsigned long Length;// Offset=0x0 Size=0x4
    struct _STRING * FileName;// Offset=0x4 Size=0x4
    enum _FILE_INFORMATION_CLASS FileInformationClass;// Offset=0x8 Size=0x4
};

struct _IO_STACK_LOCATION::__unnamed::__unnamed// Size=0x8 (Id=1703)
{
    unsigned long Length;// Offset=0x0 Size=0x4
    enum _FILE_INFORMATION_CLASS FileInformationClass;// Offset=0x4 Size=0x4
};

struct _IO_STACK_LOCATION::__unnamed::__unnamed// Size=0xc (Id=1704)
{
    unsigned long Length;// Offset=0x0 Size=0x4
    enum _FILE_INFORMATION_CLASS FileInformationClass;// Offset=0x4 Size=0x4
    struct _FILE_OBJECT * FileObject;// Offset=0x8 Size=0x4
};

struct _IO_STACK_LOCATION::__unnamed::__unnamed// Size=0x8 (Id=1705)
{
    unsigned long Length;// Offset=0x0 Size=0x4
    enum _FSINFOCLASS FsInformationClass;// Offset=0x4 Size=0x4
};

struct _IO_STACK_LOCATION::__unnamed::__unnamed// Size=0x10 (Id=1706)
{
    unsigned long OutputBufferLength;// Offset=0x0 Size=0x4
    void * InputBuffer;// Offset=0x4 Size=0x4
    unsigned long InputBufferLength;// Offset=0x8 Size=0x4
    unsigned long FsControlCode;// Offset=0xc Size=0x4
};

struct _IO_STACK_LOCATION::__unnamed::__unnamed// Size=0x10 (Id=1707)
{
    unsigned long OutputBufferLength;// Offset=0x0 Size=0x4
    void * InputBuffer;// Offset=0x4 Size=0x4
    unsigned long InputBufferLength;// Offset=0x8 Size=0x4
    unsigned long IoControlCode;// Offset=0xc Size=0x4
};

struct _IO_STACK_LOCATION::__unnamed::__unnamed// Size=0x4 (Id=1708)
{
    struct _SCSI_REQUEST_BLOCK * Srb;// Offset=0x0 Size=0x4
};

struct _IO_STACK_LOCATION::__unnamed::__unnamed// Size=0x10 (Id=1709)
{
    unsigned long Length;// Offset=0x0 Size=0x4
    unsigned char * Buffer;// Offset=0x4 Size=0x4
    unsigned long SectorNumber;// Offset=0x8 Size=0x4
    unsigned long BufferOffset;// Offset=0xc Size=0x4
};

struct _IO_STACK_LOCATION::__unnamed::__unnamed// Size=0x10 (Id=1710)
{
    void * Argument1;// Offset=0x0 Size=0x4
    void * Argument2;// Offset=0x4 Size=0x4
    void * Argument3;// Offset=0x8 Size=0x4
    void * Argument4;// Offset=0xc Size=0x4
};

union _IO_STACK_LOCATION::__unnamed// Size=0x10 (Id=1711)
{
    struct _IO_STACK_LOCATION::__unnamed::__unnamed Create;// Offset=0x0 Size=0x10
    struct _IO_STACK_LOCATION::__unnamed::__unnamed Read;// Offset=0x0 Size=0x10
    struct _IO_STACK_LOCATION::__unnamed::__unnamed Write;// Offset=0x0 Size=0x10
    struct _IO_STACK_LOCATION::__unnamed::__unnamed QueryDirectory;// Offset=0x0 Size=0xc
    struct _IO_STACK_LOCATION::__unnamed::__unnamed QueryFile;// Offset=0x0 Size=0x8
    struct _IO_STACK_LOCATION::__unnamed::__unnamed SetFile;// Offset=0x0 Size=0xc
    struct _IO_STACK_LOCATION::__unnamed::__unnamed QueryVolume;// Offset=0x0 Size=0x8
    struct _IO_STACK_LOCATION::__unnamed::__unnamed SetVolume;// Offset=0x0 Size=0x8
    struct _IO_STACK_LOCATION::__unnamed::__unnamed FileSystemControl;// Offset=0x0 Size=0x10
    struct _IO_STACK_LOCATION::__unnamed::__unnamed DeviceIoControl;// Offset=0x0 Size=0x10
    struct _IO_STACK_LOCATION::__unnamed::__unnamed Scsi;// Offset=0x0 Size=0x4
    struct _IO_STACK_LOCATION::__unnamed::__unnamed IdexReadWrite;// Offset=0x0 Size=0x10
    struct _IO_STACK_LOCATION::__unnamed::__unnamed Others;// Offset=0x0 Size=0x10
};

struct _IO_STACK_LOCATION// Size=0x24 (Id=1712)
{
    unsigned char MajorFunction;// Offset=0x0 Size=0x1
    unsigned char MinorFunction;// Offset=0x1 Size=0x1
    unsigned char Flags;// Offset=0x2 Size=0x1
    unsigned char Control;// Offset=0x3 Size=0x1
    union _IO_STACK_LOCATION::__unnamed Parameters;// Offset=0x4 Size=0x10
    struct _DEVICE_OBJECT * DeviceObject;// Offset=0x14 Size=0x4
    struct _FILE_OBJECT * FileObject;// Offset=0x18 Size=0x4
    long  ( * CompletionRoutine)(struct _DEVICE_OBJECT * ,struct _IRP * ,void * );// Offset=0x1c Size=0x4
    void * Context;// Offset=0x20 Size=0x4
};

struct _MODE_SENSE// Size=0x6 (Id=2323)
{
    unsigned char OperationCode;// Offset=0x0 Size=0x1
    struct // Size=0x2 (Id=0)
    {
        unsigned char Reserved1:3;// Offset=0x1 Size=0x1 BitOffset=0x0 BitSize=0x3
        unsigned char Dbd:1;// Offset=0x1 Size=0x1 BitOffset=0x3 BitSize=0x1
        unsigned char Reserved2:1;// Offset=0x1 Size=0x1 BitOffset=0x4 BitSize=0x1
        unsigned char LogicalUnitNumber:3;// Offset=0x1 Size=0x1 BitOffset=0x5 BitSize=0x3
        unsigned char PageCode:6;// Offset=0x2 Size=0x1 BitOffset=0x0 BitSize=0x6
        unsigned char Pc:2;// Offset=0x2 Size=0x1 BitOffset=0x6 BitSize=0x2
    };
    unsigned char Reserved3;// Offset=0x3 Size=0x1
    unsigned char AllocationLength;// Offset=0x4 Size=0x1
    unsigned char Control;// Offset=0x5 Size=0x1
};

struct _MODE_SENSE10// Size=0xa (Id=2334)
{
    unsigned char OperationCode;// Offset=0x0 Size=0x1
    struct // Size=0x2 (Id=0)
    {
        unsigned char Reserved1:3;// Offset=0x1 Size=0x1 BitOffset=0x0 BitSize=0x3
        unsigned char Dbd:1;// Offset=0x1 Size=0x1 BitOffset=0x3 BitSize=0x1
        unsigned char Reserved2:1;// Offset=0x1 Size=0x1 BitOffset=0x4 BitSize=0x1
        unsigned char LogicalUnitNumber:3;// Offset=0x1 Size=0x1 BitOffset=0x5 BitSize=0x3
        unsigned char PageCode:6;// Offset=0x2 Size=0x1 BitOffset=0x0 BitSize=0x6
        unsigned char Pc:2;// Offset=0x2 Size=0x1 BitOffset=0x6 BitSize=0x2
    };
    unsigned char Reserved3[4];// Offset=0x3 Size=0x4
    unsigned char AllocationLength[2];// Offset=0x7 Size=0x2
    unsigned char Control;// Offset=0x9 Size=0x1
};

struct _MODE_SELECT// Size=0x6 (Id=2338)
{
    unsigned char OperationCode;// Offset=0x0 Size=0x1
    struct // Size=0x1 (Id=0)
    {
        unsigned char SPBit:1;// Offset=0x1 Size=0x1 BitOffset=0x0 BitSize=0x1
        unsigned char Reserved1:3;// Offset=0x1 Size=0x1 BitOffset=0x1 BitSize=0x3
        unsigned char PFBit:1;// Offset=0x1 Size=0x1 BitOffset=0x4 BitSize=0x1
        unsigned char LogicalUnitNumber:3;// Offset=0x1 Size=0x1 BitOffset=0x5 BitSize=0x3
    };
    unsigned char Reserved2[2];// Offset=0x2 Size=0x2
    unsigned char ParameterListLength;// Offset=0x4 Size=0x1
    unsigned char Control;// Offset=0x5 Size=0x1
};

struct _MODE_PARAMETER_HEADER10// Size=0x8 (Id=2344)
{
    unsigned char ModeDataLength[2];// Offset=0x0 Size=0x2
    unsigned char MediumType;// Offset=0x2 Size=0x1
    unsigned char DeviceSpecificParameter;// Offset=0x3 Size=0x1
    unsigned char Reserved[2];// Offset=0x4 Size=0x2
    unsigned char BlockDescriptorLength[2];// Offset=0x6 Size=0x2
};

struct _MODE_SELECT10// Size=0xa (Id=2363)
{
    unsigned char OperationCode;// Offset=0x0 Size=0x1
    struct // Size=0x1 (Id=0)
    {
        unsigned char SPBit:1;// Offset=0x1 Size=0x1 BitOffset=0x0 BitSize=0x1
        unsigned char Reserved1:3;// Offset=0x1 Size=0x1 BitOffset=0x1 BitSize=0x3
        unsigned char PFBit:1;// Offset=0x1 Size=0x1 BitOffset=0x4 BitSize=0x1
        unsigned char LogicalUnitNumber:3;// Offset=0x1 Size=0x1 BitOffset=0x5 BitSize=0x3
    };
    unsigned char Reserved2[5];// Offset=0x2 Size=0x5
    unsigned char ParameterListLength[2];// Offset=0x7 Size=0x2
    unsigned char Control;// Offset=0x9 Size=0x1
};

struct _RTL_CRITICAL_SECTION// Size=0x1c (Id=3053)
{
    union __unnamed Synchronization;// Offset=0x0 Size=0x10
    long LockCount;// Offset=0x10 Size=0x4
    long RecursionCount;// Offset=0x14 Size=0x4
    void * OwningThread;// Offset=0x18 Size=0x4
};

struct _KDPC// Size=0x1c (Id=3055)
{
    short Type;// Offset=0x0 Size=0x2
    unsigned char Inserted;// Offset=0x2 Size=0x1
    unsigned char Padding;// Offset=0x3 Size=0x1
    struct _LIST_ENTRY DpcListEntry;// Offset=0x4 Size=0x8
    void  ( * DeferredRoutine)(struct _KDPC * ,void * ,void * ,void * );// Offset=0xc Size=0x4
    void * DeferredContext;// Offset=0x10 Size=0x4
    void * SystemArgument1;// Offset=0x14 Size=0x4
    void * SystemArgument2;// Offset=0x18 Size=0x4
};

struct _KAPC// Size=0x28 (Id=3056)
{
    short Type;// Offset=0x0 Size=0x2
    char ApcMode;// Offset=0x2 Size=0x1
    unsigned char Inserted;// Offset=0x3 Size=0x1
    struct _KTHREAD * Thread;// Offset=0x4 Size=0x4
    struct _LIST_ENTRY ApcListEntry;// Offset=0x8 Size=0x8
    void  ( * KernelRoutine)(struct _KAPC * ,void  ( ** )(void * ,void * ,void * ),void ** ,void ** ,void ** );// Offset=0x10 Size=0x4
    void  ( * RundownRoutine)(struct _KAPC * );// Offset=0x14 Size=0x4
    void  ( * NormalRoutine)(void * ,void * ,void * );// Offset=0x18 Size=0x4
    void * NormalContext;// Offset=0x1c Size=0x4
    void * SystemArgument1;// Offset=0x20 Size=0x4
    void * SystemArgument2;// Offset=0x24 Size=0x4
};


#endif // _WINDOWS_KERNEL_H_
