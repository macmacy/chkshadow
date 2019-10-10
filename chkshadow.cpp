/*
 * The program searches for system Event object, which name starts with 'RDPSchedulerEvent'
 * in \Sessions\<SID>\BaseNamedObjects directory, * where SID is Windows session identifier.
 *
 * Looks lik RdpSa.exe program creates this event when session shadowing is in progress.
 */
#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <winternl.h>

/* Set from command line for more output
 */ 
static bool verbose = false;

static NTSTATUS (NTAPI* _NtOpenDirectoryObject)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES);
static NTSTATUS (NTAPI* _NtQueryDirectoryObject)(HANDLE, PVOID, ULONG, BOOLEAN, BOOLEAN, PULONG, PULONG);
static VOID     (NTAPI* _RtlInitUnicodeString)(PUNICODE_STRING, PCWSTR);
static NTSTATUS (NTAPI* _NtClose)(HANDLE);

#define DIRECTORY_QUERY                 (0x0001)
#define DIRECTORY_TRAVERSE              (0x0002)

typedef struct _OBJECT_DIRECTORY_INFORMATION {
    UNICODE_STRING Name;
    UNICODE_STRING TypeName;
} OBJECT_DIRECTORY_INFORMATION, *POBJECT_DIRECTORY_INFORMATION;

#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS                   ((NTSTATUS)0x00000000L) 
#endif 
#ifndef STATUS_MORE_ENTRIES
#define STATUS_MORE_ENTRIES              ((NTSTATUS)0x00000105L)
#endif 
#ifndef STATUS_NO_MORE_ENTRIES
#define STATUS_NO_MORE_ENTRIES           ((NTSTATUS)0x8000001AL)
#endif 
#ifndef STATUS_BUFFER_TOO_SMALL 
#define STATUS_BUFFER_TOO_SMALL          ((NTSTATUS)0xC0000023L)
#endif


typedef  void (*EnumCallback)(POBJECT_DIRECTORY_INFORMATION);

bool EnumObjects( PCWSTR Dir, PCWSTR Type, int &matchingTypeCount, PCWSTR searchedEventName, int &matchingNameCount, EnumCallback callback )
{
  bool result = true;
  NTSTATUS ntStatus;
  OBJECT_ATTRIBUTES oa;
  UNICODE_STRING objname;
  HANDLE dirHandle = NULL;

  matchingTypeCount = matchingNameCount = 0;
  
  _RtlInitUnicodeString(&objname, Dir  );
  InitializeObjectAttributes(&oa, &objname, 0, NULL, NULL);
  ntStatus = _NtOpenDirectoryObject(&dirHandle, DIRECTORY_QUERY | DIRECTORY_TRAVERSE, &oa);
  if( NT_SUCCESS(ntStatus)) {
    ULONG context = 0, bytes = 0;
    BOOLEAN firstCall = TRUE;
    POBJECT_DIRECTORY_INFORMATION infoBuffer = NULL;
    
    bool list = true;
    while (list) {
      // First call with size 0, to get necessary buffer size
      // This behaviour is not documented for NtQueryDirectoryObject, but seems it works the same as  other Win32 API functions
      ntStatus = _NtQueryDirectoryObject(dirHandle, infoBuffer, 0L, FALSE, firstCall, &context, &bytes);
      if (ntStatus == STATUS_BUFFER_TOO_SMALL) {
        SIZE_T bufferSize = bytes;
        infoBuffer = new OBJECT_DIRECTORY_INFORMATION[bufferSize];
        if ( infoBuffer ) {
          ntStatus = _NtQueryDirectoryObject(dirHandle, infoBuffer, bufferSize, FALSE, firstCall, &context, &bytes);
          if( NT_SUCCESS(ntStatus) ) {
            ULONG i=0;            
            while ( infoBuffer[i].Name.Buffer ) {
              POBJECT_DIRECTORY_INFORMATION info = &infoBuffer[i];              
              if ( callback ) {
                callback( info );
              }
              if( wcsncmp(info->TypeName.Buffer, Type, info->TypeName.Length / sizeof(WCHAR)) == 0) {
                if ( wcsstr( info->Name.Buffer, searchedEventName) != NULL ) {
                  matchingNameCount++;
                }
                matchingTypeCount++;
              }
              i++;            
            }
          }      

          if ( ntStatus != STATUS_MORE_ENTRIES) {
            list = false;
          }
          delete infoBuffer;
        }
      }
      else {
        list = false;
        result = false;
      }
      firstCall = FALSE;
    }
    _NtClose(dirHandle);

  }
  else {
    result = false;
  }
  return result;
}




void  printObject(POBJECT_DIRECTORY_INFORMATION info)
{
  static int ord = 0;
  if ( verbose ) {
    printf( _T("TYPE[%d]: %S\n"), ord, info->TypeName.Buffer);
    printf( _T("NAME[%d]: %S\n"), ord, info->Name.Buffer);           
    printf("\n");    
  }
  ord++;
}

#define INVALID_SESSION_ID 0xFFFFFFFF


int _tmain(int argc, char** argv)
{
  int ec = 1;
  DWORD sid = INVALID_SESSION_ID;

  if ( argc > 1 ) {
    // Very, very, very primitive command-line "parsing":
    
    char *end;
    sid = strtol( argv[1], &end, 10 );
    // On successful conversion, strtol should point to end of string:
    if ( *end == 0 ) {
      printf("Checking windows session %d...\n", sid);
      if ( argc > 2 ) {
        if ( strcmp( argv[2], "v" ) == 0 ) {
          verbose = true;
        }
      }
    }
    else {
      printf("Incorrect parameter.\n");
      printf("Usage: program <session-number> [v]\n");
      sid = INVALID_SESSION_ID;
    }
  }
  else {
    DWORD currentPid = GetCurrentProcessId();
    ProcessIdToSessionId(currentPid, &sid );
    printf("Checking current windows session (%d)...\n", sid);
  }

  if ( sid != INVALID_SESSION_ID ) {
    HMODULE hNtDll = ::GetModuleHandle(_T("ntdll.dll"));
    *(FARPROC*)&_NtOpenDirectoryObject = ::GetProcAddress(hNtDll, "NtOpenDirectoryObject");
    *(FARPROC*)&_NtQueryDirectoryObject = ::GetProcAddress(hNtDll, "NtQueryDirectoryObject");
    *(FARPROC*)&_RtlInitUnicodeString = ::GetProcAddress(hNtDll, "RtlInitUnicodeString");
    *(FARPROC*)&_NtClose = ::GetProcAddress(hNtDll, "NtClose");
  
    if ( _NtOpenDirectoryObject && _NtQueryDirectoryObject && _RtlInitUnicodeString && _NtClose ) {
      wchar_t Dir[512];
      swprintf( Dir, sizeof(Dir), L"\\Sessions\\%d\\BaseNamedObjects", sid );
      int objectsMatchingType = 0;
      int objectsMatchingName = 0;

      PCWSTR ObjectType = L"Event";

      printf( _T("\nListing '%S', type: '%S'\n"), Dir, ObjectType );
      if ( EnumObjects( Dir , ObjectType, objectsMatchingType, L"RDPSchedulerEvent", objectsMatchingName, printObject ) ) {
        ec = 0;
        printf("Found objects of '%S' type: %d\n", ObjectType, objectsMatchingType );
        if ( objectsMatchingName ) {
          printf("*** Session %d looks like being shadowed! ***\n", sid);
        }
        else {
          printf("Shadowing not detected for session %d.\n", sid);
        }
      } else {
        printf("Failed to enumarate objects\n");
      }
    } else {      
      printf(_T("Failed to retrieve ntdll.dll function pointers\n"));
    }
  }
  return ec;
}
