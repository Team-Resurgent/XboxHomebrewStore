//=============================================================================
// Store.cpp - Xbox Homebrew Store Implementation
//=============================================================================

#include "Main.h"
#include "Store.h"

// Colors - Modern clean theme inspired by Switch store
#define COLOR_PRIMARY       0xFFE91E63  // Pink/Red accent
#define COLOR_SECONDARY     0xFF424242  // Dark gray
#define COLOR_BG            0xFF212121  // Very dark gray
#define COLOR_CARD_BG       0xFF303030  // Card background
#define COLOR_WHITE         0xFFFFFFFF
#define COLOR_TEXT_GRAY     0xFFB0B0B0
#define COLOR_SUCCESS       0xFF4CAF50  // Green for installed
#define COLOR_DOWNLOAD      0xFF2196F3  // Blue for download
#define COLOR_NEW           0xFFFF1744  // Bright red for NEW items
#define COLOR_SIDEBAR       0xFFD81B60  // Sidebar pink
#define COLOR_SIDEBAR_HOVER 0xFFC2185B  // Darker pink for selected

// File paths
#define CATALOG_PATH        "D:\\Media\\store.json"
#define USER_STATE_PATH     "T:\\user_state.json"

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
Store::Store()
{
    m_pItems = NULL;
    m_nItemCount = 0;
    m_nSelectedItem = 0;
    m_nFilteredCount = 0;
    m_CurrentState = UI_MAIN_GRID;
    m_nCategoryCount = 0;
    m_nSelectedCategory = 0;
    m_pVB = NULL;
    m_pGamepads = NULL;
    m_dwLastButtons = 0;
	m_pFilteredIndices = NULL;
    
    // Will be set in Initialize
    m_fScreenWidth = 640.0f;
    m_fScreenHeight = 480.0f;
    m_fSidebarWidth = 200.0f;
    m_nGridCols = 3;
    m_nGridRows = 2;
    
    // Initialize first category as "All Apps"
    strcpy( m_aCategories[0].szName, "All Apps" );
    strcpy( m_aCategories[0].szID, "all" );
    m_aCategories[0].nAppCount = 0;
    m_nCategoryCount = 1;
}

//-----------------------------------------------------------------------------
// Destructor
//-----------------------------------------------------------------------------
Store::~Store()
{
    if( m_pVB )
        m_pVB->Release();
    
    m_Font.Destroy();

	if (m_pFilteredIndices)
		delete[] m_pFilteredIndices;

    if( m_pItems )
    {
        // Release item textures
        for( int i = 0; i < m_nItemCount; i++ )
        {
            if( m_pItems[i].pIcon )
                m_pItems[i].pIcon->Release();
        }
        delete[] m_pItems;
    }
}

//-----------------------------------------------------------------------------
// Simple JSON helper - find value for a key
//-----------------------------------------------------------------------------
const char* Store::FindJSONValue( const char* json, const char* key )
{
    static char value[512];
    value[0] = '\0';
    
    char searchKey[128];
    sprintf( searchKey, "\"%s\"", key );
    
    const char* keyPos = strstr( json, searchKey );
    if( !keyPos ) return value;
    
    const char* colonPos = strchr( keyPos, ':' );
    if( !colonPos ) return value;
    
    // Skip whitespace after colon
    colonPos++;
    while( *colonPos == ' ' || *colonPos == '\t' ) colonPos++;
    
    // Handle string values (in quotes) - NOW WITH ESCAPE SUPPORT!
	if( *colonPos == '"' )
	{
		colonPos++;
		int i = 0;
		while( *colonPos && i < 510 )
		{
			// Handle escaped characters
			if( *colonPos == '\\' && *(colonPos + 1) )
			{
				// Copy the backslash and the next character as-is
				value[i++] = *colonPos++;
				if( i < 510 && *colonPos )
					value[i++] = *colonPos++;
			}
			// Stop at unescaped quote
			else if( *colonPos == '"' )
			{
				break;  // Only stop at REAL end quote
			}
			else
			{
				value[i++] = *colonPos++;
			}
		}
		value[i] = '\0';
	}
    // Handle boolean values (true/false)
    else if( strncmp( colonPos, "true", 4 ) == 0 )
    {
        strcpy( value, "true" );
    }
    else if( strncmp( colonPos, "false", 5 ) == 0 )
    {
        strcpy( value, "false" );
    }
    // Handle numeric values
    else
    {
        int i = 0;
        while( *colonPos && (*colonPos == '-' || (*colonPos >= '0' && *colonPos <= '9')) && i < 510 )
        {
            value[i++] = *colonPos++;
        }
        value[i] = '\0';
    }
    
    return value;
}

//-----------------------------------------------------------------------------
// SafeCopy - Safe string copy with null termination
//-----------------------------------------------------------------------------
void Store::SafeCopy(char* dest, const char* src, int maxLen)
{
    if (!dest || maxLen <= 0)
        return;

    if (!src)
    {
        dest[0] = '\0';
        return;
    }

    strncpy(dest, src, maxLen - 1);
    dest[maxLen - 1] = '\0';
}

//-----------------------------------------------------------------------------
// SafeCopyUTF8 - UTF-8 aware string copy
//-----------------------------------------------------------------------------
void Store::SafeCopyUTF8(char* dest, const char* src, int maxBytes)
{
    if (!dest || maxBytes <= 0)
        return;
    
    if (!src)
    {
        dest[0] = '\0';
        return;
    }
    
    int bytesWritten = 0;
    const unsigned char* s = (const unsigned char*)src;
    
    while (*s && bytesWritten < maxBytes - 4)  // -4 for max UTF-8 char size
    {
        if (*s < 0x80)
        {
            // ASCII (1 byte)
            dest[bytesWritten++] = *s++;
        }
        else if ((*s & 0xE0) == 0xC0)
        {
            // 2-byte UTF-8
            if (bytesWritten + 2 < maxBytes && s[1])
            {
                dest[bytesWritten++] = *s++;
                dest[bytesWritten++] = *s++;
            }
            else break;
        }
        else if ((*s & 0xF0) == 0xE0)
        {
            // 3-byte UTF-8
            if (bytesWritten + 3 < maxBytes && s[1] && s[2])
            {
                dest[bytesWritten++] = *s++;
                dest[bytesWritten++] = *s++;
                dest[bytesWritten++] = *s++;
            }
            else break;
        }
        else if ((*s & 0xF8) == 0xF0)
        {
            // 4-byte UTF-8
            if (bytesWritten + 4 < maxBytes && s[1] && s[2] && s[3])
            {
                dest[bytesWritten++] = *s++;
                dest[bytesWritten++] = *s++;
                dest[bytesWritten++] = *s++;
                dest[bytesWritten++] = *s++;
            }
            else break;
        }
        else
        {
            // Invalid UTF-8, skip
            s++;
        }
    }
    
    dest[bytesWritten] = '\0';
}

//-----------------------------------------------------------------------------
// UnescapeJSON - Handle JSON escape sequences
//-----------------------------------------------------------------------------
void Store::UnescapeJSON(char* dest, const char* src, int maxLen)
{
    if (!dest || !src || maxLen <= 0)
    {
        if (dest && maxLen > 0)
            dest[0] = '\0';
        return;
    }
    
    int j = 0;
    for (int i = 0; src[i] && j < maxLen - 1; i++)
    {
        if (src[i] == '\\' && src[i+1])
        {
            switch (src[i+1])
            {
                case 'n':  dest[j++] = '\n'; i++; break;
                case 't':  dest[j++] = '\t'; i++; break;
                case 'r':  dest[j++] = '\r'; i++; break;
                case '\\': dest[j++] = '\\'; i++; break;
                case '"':  dest[j++] = '"';  i++; break;
                case '/':  dest[j++] = '/';  i++; break;
                case 'b':  dest[j++] = '\b'; i++; break;
                case 'f':  dest[j++] = '\f'; i++; break;
                default:   dest[j++] = src[i]; break;
            }
        }
        else
        {
            dest[j++] = src[i];
        }
    }
    dest[j] = '\0';
}

//-----------------------------------------------------------------------------
// FindJSONValueUnescaped - Get JSON value with escapes handled
//-----------------------------------------------------------------------------
const char* Store::FindJSONValueUnescaped(const char* json, const char* key)
{
    static char unescaped[1024];
    const char* escaped = FindJSONValue(json, key);
    UnescapeJSON(unescaped, escaped, sizeof(unescaped));
    return unescaped;
}


//-----------------------------------------------------------------------------
// SafeParseInt - Safe integer parsing with bounds checking
//-----------------------------------------------------------------------------
long Store::SafeParseInt(const char* str, long defaultVal, long minVal, long maxVal)
{
    if (!str || !str[0])
        return defaultVal;
    
    // Skip whitespace
    while (*str == ' ' || *str == '\t') str++;
    
    // Check for valid number
    const char* p = str;
    if (*p == '-' || *p == '+') p++;
    
    BOOL hasDigit = FALSE;
    while (*p)
    {
        if (*p >= '0' && *p <= '9')
        {
            hasDigit = TRUE;
            p++;
        }
        else
        {
            return defaultVal;  // Invalid character
        }
    }
    
    if (!hasDigit)
        return defaultVal;
    
    // Parse
    char* endPtr;
    long val = strtol(str, &endPtr, 10);
    
    // Clamp to range
    if (val < minVal) return minVal;
    if (val > maxVal) return maxVal;
    
    return val;
}


//-----------------------------------------------------------------------------
// GenerateAppID - Create lowercase no-space ID from name
//-----------------------------------------------------------------------------
void Store::GenerateAppID(const char* name, char* outID, int maxLen)
{
    if (!name || !outID || maxLen <= 0)
        return;

    int j = 0;

    for (const char* s = name; *s && j < maxLen - 1; s++)
    {
        if (*s != ' ')
        {
            if (*s >= 'A' && *s <= 'Z')
                outID[j++] = *s + 32;
            else
                outID[j++] = *s;
        }
    }

    outID[j] = '\0';
}

//-----------------------------------------------------------------------------
// GetOrCreateCategory - Find or create a category index (SAFE)
//-----------------------------------------------------------------------------
int Store::GetOrCreateCategory(const char* catID)
{
    if (!catID || catID[0] == '\0')
        return 0;

    // Check existing categories (skip 0 - "All Apps")
    for (int i = 1; i < m_nCategoryCount; i++)
    {
        if (strcmp(m_aCategories[i].szID, catID) == 0)
            return i;
    }

    if (m_nCategoryCount >= MAX_CATEGORIES)
    {
        OutputDebugString("Warning: Max categories reached\n");
        return 1;
    }

    int newIndex = m_nCategoryCount++;

    SafeCopy(m_aCategories[newIndex].szID, catID, sizeof(m_aCategories[newIndex].szID));
    SafeCopy(m_aCategories[newIndex].szName, catID, sizeof(m_aCategories[newIndex].szName));

    // Capitalize first letter
    if (m_aCategories[newIndex].szName[0] >= 'a' &&
        m_aCategories[newIndex].szName[0] <= 'z')
    {
        m_aCategories[newIndex].szName[0] -= 32;
    }

    m_aCategories[newIndex].nAppCount = 0;

    return newIndex;
}

//-----------------------------------------------------------------------------
// BuildCategoryList - Count apps in each category
//-----------------------------------------------------------------------------
void Store::BuildCategoryList()
{
    // Reset counts
    for( int i = 0; i < m_nCategoryCount; i++ )
        m_aCategories[i].nAppCount = 0;
    
    // Count apps in each category
    for( int i = 0; i < m_nItemCount; i++ )
    {
        if( m_pItems[i].nCategoryIndex >= 0 && m_pItems[i].nCategoryIndex < m_nCategoryCount )
        {
            m_aCategories[m_pItems[i].nCategoryIndex].nAppCount++;
            m_aCategories[0].nAppCount++; // "All Apps" gets everything
        }
    }
    
    char szDebug[256];
    sprintf( szDebug, "Categories: %d total\n", m_nCategoryCount );
    OutputDebugString( szDebug );
    for( int i = 0; i < m_nCategoryCount; i++ )
    {
        sprintf( szDebug, "  %s: %d apps\n", m_aCategories[i].szName, m_aCategories[i].nAppCount );
        OutputDebugString( szDebug );
    }
}

//-----------------------------------------------------------------------------
// LoadCatalogFromFile - Load and parse store.json
//-----------------------------------------------------------------------------
BOOL Store::LoadCatalogFromFile(const char* filename)
{
    FILE* f = fopen(filename, "rb");
    if (!f)
    {
        OutputDebugString("Failed to open catalog file\n");
        return FALSE;
    }

    fseek(f, 0, SEEK_END);
    long fileSize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (fileSize <= 0 || fileSize > 10 * 1024 * 1024)
    {
        fclose(f);
        return FALSE;
    }

	char* jsonData = new char[fileSize + 1];
	if (!jsonData)
	{
		OutputDebugString("ERROR: Out of memory loading catalog\n");
		fclose(f);
		return FALSE;
	}
	fread(jsonData, 1, fileSize, f);

    const char* appsStart = strstr(jsonData, "\"apps\"");
    if (!appsStart)
    {
        delete[] jsonData;
        return FALSE;
    }

    const char* arrayStart = strchr(appsStart, '[');
    if (!arrayStart)
    {
        delete[] jsonData;
        return FALSE;
    }

    // -------------------------------------------------
    // PASS 1: COUNT TOP-LEVEL APP OBJECTS
    // -------------------------------------------------
    int appCount = 0;
    const char* p = arrayStart + 1;
    int depth = 0;

    while (*p)
    {
        if (*p == '{')
        {
            if (depth == 0)
                appCount++;  // Only count direct children of apps array

            depth++;
        }
        else if (*p == '}')
        {
            depth--;
        }
        else if (*p == ']' && depth == 0)
        {
            break;
        }

        p++;
    }

    // Safety limit check
    #define MAX_CATALOG_APPS 3000
    
    if (appCount <= 0)
    {
        delete[] jsonData;
        return FALSE;
    }

    if (appCount > MAX_CATALOG_APPS)
    {
        char szErr[256];
        sprintf(szErr, "ERROR: Catalog has %d apps, max supported is %d\n", 
                appCount, MAX_CATALOG_APPS);
        OutputDebugString(szErr);
        delete[] jsonData;
        return FALSE;
    }

    m_nItemCount = appCount;
    m_pItems = new StoreItem[m_nItemCount];
    if (!m_pItems)
    {
        OutputDebugString("CRITICAL: Out of memory allocating items\n");
        m_nItemCount = 0;
        delete[] jsonData;
        return FALSE;
    }

    // Zero-initialize for safety
    memset(m_pItems, 0, sizeof(StoreItem) * m_nItemCount);

    // Allocate filtered index buffer
    if (m_pFilteredIndices)
    {
        delete[] m_pFilteredIndices;
        m_pFilteredIndices = NULL;
    }

    m_pFilteredIndices = new int[m_nItemCount];
    if (!m_pFilteredIndices)
    {
        OutputDebugString("CRITICAL: Out of memory allocating filtered indices\n");
        delete[] m_pItems;
        m_pItems = NULL;
        m_nItemCount = 0;
        delete[] jsonData;
        return FALSE;
    }

    // -------------------------------------------------
    // PASS 2: PARSE EACH APP OBJECT
    // -------------------------------------------------
    p = arrayStart + 1;
    int itemIndex = 0;
    depth = 0;

    while (*p && itemIndex < m_nItemCount)
    {
        if (*p == '{' && depth == 0)
        {
            const char* objStart = p;
            int braceDepth = 0;
            const char* scan = p;
            const char* objEnd = NULL;

            while (*scan)
            {
                if (*scan == '{') braceDepth++;
                else if (*scan == '}')
                {
                    braceDepth--;
                    if (braceDepth == 0)
                    {
                        objEnd = scan;
                        break;
                    }
                }
                scan++;
            }

            if (!objEnd)
                break;

            int objLen = objEnd - objStart + 1;
            char* objData = new char[objLen + 1];
            memcpy(objData, objStart, objLen);
            objData[objLen] = '\0';

            StoreItem* item = &m_pItems[itemIndex];

            // Use UTF-8 aware copy for names and descriptions (international support)
            SafeCopyUTF8(item->szName, FindJSONValueUnescaped(objData, "name"), sizeof(item->szName));
            SafeCopyUTF8(item->szAuthor, FindJSONValueUnescaped(objData, "author"), sizeof(item->szAuthor));
            SafeCopyUTF8(item->szDescription, FindJSONValueUnescaped(objData, "description"), sizeof(item->szDescription));
            SafeCopy(item->szGUID, FindJSONValue(objData, "guid"), sizeof(item->szGUID));
			SafeCopy(item->szID, FindJSONValue(objData, "id"), sizeof(item->szID));

            item->nCategoryIndex = GetOrCreateCategory(FindJSONValue(objData, "category"));
            item->pIcon = NULL;
            item->nVersionCount = 0;
            item->nSelectedVersion = 0;
            item->nVersionScrollOffset = 0;
            item->bViewingVersionDetail = FALSE;

            const char* newFlag = FindJSONValue(objData, "new");
            item->bNew = (strcmp(newFlag, "true") == 0 || strcmp(newFlag, "True") == 0);

            // -------------------------
            // Parse Versions
            // -------------------------
            const char* versionsStart = strstr(objData, "\"versions\"");
            if (versionsStart)
            {
                const char* verArray = strchr(versionsStart, '[');
                if (verArray)
                {
                    const char* vp = verArray + 1;

                    while (*vp && item->nVersionCount < MAX_VERSIONS)
                    {
                        if (*vp == '{')
                        {
                            const char* vStart = vp;
                            int vDepth = 0;
                            const char* vScan = vp;
                            const char* vEnd = NULL;

                            while (*vScan)
                            {
                                if (*vScan == '{') vDepth++;
                                else if (*vScan == '}')
                                {
                                    vDepth--;
                                    if (vDepth == 0)
                                    {
                                        vEnd = vScan;
                                        break;
                                    }
                                }
                                vScan++;
                            }

                            if (!vEnd)
                                break;

                            int vLen = vEnd - vStart + 1;
                            char* vData = new char[vLen + 1];
                            memcpy(vData, vStart, vLen);
                            vData[vLen] = '\0';

                            VersionInfo* ver = &item->aVersions[item->nVersionCount];

                            SafeCopy(ver->szVersion, FindJSONValue(vData, "version"), sizeof(ver->szVersion));

							// FIXED: Proper changelog parsing with escapes
							const char* changelog = FindJSONValue(vData, "changelog");
							char unescapedChangelog[256];
							UnescapeJSON(unescapedChangelog, changelog, sizeof(unescapedChangelog));
							SafeCopyUTF8(ver->szChangelog, unescapedChangelog, sizeof(ver->szChangelog));

							SafeCopy(ver->szReleaseDate, FindJSONValue(vData, "release_date"), sizeof(ver->szReleaseDate));
							SafeCopy(ver->szTitleID, FindJSONValue(vData, "title_id"), sizeof(ver->szTitleID));
							SafeCopy(ver->szRegion, FindJSONValue(vData, "region"), sizeof(ver->szRegion));
							SafeCopy(ver->szGUID, FindJSONValue(vData, "guid"), sizeof(ver->szGUID));

							ver->szInstallPath[0] = '\0';
                            ver->dwSize = SafeParseInt(FindJSONValue(vData, "size"), 0, 0, 2000000000);
                            ver->nState = SafeParseInt(FindJSONValue(vData, "state"), 0, 0, 3);

                            delete[] vData;
                            item->nVersionCount++;

                            vp = vEnd + 1;
                        }
                        else if (*vp == ']')
                            break;
                        else
                            vp++;
                    }
                }
            }

            delete[] objData;
            itemIndex++;
            p = objEnd + 1;
        }
        else
        {
            if (*p == '{') depth++;
            else if (*p == '}') depth--;
            p++;
        }
    }

    delete[] jsonData;

    BuildCategoryList();

    char dbg[128];
    sprintf(dbg, "Loaded %d apps from catalog\n", m_nItemCount);
    OutputDebugString(dbg);

    return TRUE;
}
//-----------------------------------------------------------------------------
// LoadUserState - Load user's personal state from T:\user_state.json
//-----------------------------------------------------------------------------
BOOL Store::LoadUserState( const char* filename )
{
    OutputDebugString( "Loading user state...\n" );
    
    FILE* f = fopen( filename, "rb" );
    if( !f )
    {
        OutputDebugString( "No user state file found - this is normal on first run\n" );
        return FALSE;
    }
    
    fseek( f, 0, SEEK_END );
    long fileSize = ftell( f );
    fseek( f, 0, SEEK_SET );
    
	char* fileData = new char[fileSize + 1];
	if (!fileData)
	{
		OutputDebugString("ERROR: Out of memory loading user state\n");
		fclose(f);
		return FALSE;
	}
	fread( fileData, 1, fileSize, f );
	fileData[fileSize] = '\0';
	fclose(f);

    // Parse app states
    for( int i = 0; i < m_nItemCount; i++ )
    {
		// Use ID as app identifier (unique per app, safe to expose)
		const char* appId = m_pItems[i].szID;
		if (!appId || appId[0] == '\0')
		{
			// Fallback to GUID if no ID (backwards compatibility)
			appId = m_pItems[i].szGUID;
			if (!appId || appId[0] == '\0')
				continue;
		}
        
        // Find this app in JSON
        char searchStr[128];
        sprintf( searchStr, "\"%s\"", appId );
        const char* appPos = strstr( fileData, searchStr );
        if( !appPos ) continue;
        
        // Check viewed flag
        const char* viewedPos = strstr( appPos, "\"viewed\"" );
        if( viewedPos )
        {
            if( strstr( viewedPos, "true" ) )
                m_pItems[i].bNew = FALSE;
        }
        
        // Parse version states
        for( int v = 0; v < m_pItems[i].nVersionCount; v++ )
        {
            char verSearch[128];
            sprintf( verSearch, "\"%s\"", m_pItems[i].aVersions[v].szVersion );
            const char* verPos = strstr( appPos, verSearch );
            if( verPos )
            {
                const char* statePos = strstr( verPos, "\"state\"" );
                if( statePos )
                {
                    const char* colonPos = strchr( statePos, ':' );
                    if( colonPos )
                    {
                        m_pItems[i].aVersions[v].nState = atoi( colonPos + 1 );
                    }
                }
                
                // Load install path if present
                const char* pathPos = strstr( verPos, "\"path\"" );
                if( pathPos )
                {
                    const char* pathColon = strchr( pathPos, ':' );
                    if( pathColon )
                    {
                        pathColon++;
                        while( *pathColon == ' ' || *pathColon == '"' ) pathColon++;
                        
                        const char* pathEnd = strchr( pathColon, '"' );
                        if( pathEnd )
                        {
                            int pathLen = pathEnd - pathColon;
                            if( pathLen >= 128 ) pathLen = 127;
                            strncpy( m_pItems[i].aVersions[v].szInstallPath, pathColon, pathLen );
                            m_pItems[i].aVersions[v].szInstallPath[pathLen] = '\0';
                        }
                    }
                }
            }
        }
    }
    
    delete[] fileData;
    OutputDebugString( "User state applied\n" );
    return TRUE;
}
//-----------------------------------------------------------------------------
// SaveUserState - Save user's state to T:\user_state.json
//-----------------------------------------------------------------------------
BOOL Store::SaveUserState( const char* filename )
{
    OutputDebugString( "Saving user state...\n" );
    
    FILE* f = fopen( filename, "wb" );
    if( !f )
    {
        char szError[256];
        sprintf( szError, "Failed to create user state file: %s\n", filename );
        OutputDebugString( szError );
        return FALSE;
    }
    
    // Write JSON manually (simple format)
    fprintf( f, "{\n" );
    fprintf( f, "  \"version\": \"1.0\",\n" );
    fprintf( f, "  \"last_updated\": \"2026-02-12\",\n" );
    
    // Write each app's state
    for( int i = 0; i < m_nItemCount; i++ )
    {
        StoreItem* pItem = &m_pItems[i];
        
		// Use ID as app identifier (not GUID - keeps download URLs secret)
		const char* appId = pItem->szID;
		if (!appId || appId[0] == '\0')
		{
			// Fallback to GUID (backwards compatibility)
			appId = pItem->szGUID;
			if (!appId || appId[0] == '\0')
				continue;
		}
        
        // Only save if user has interacted with this app
        BOOL hasState = FALSE;
        BOOL viewed = !pItem->bNew;  // If not NEW, user must have viewed it
        
        // Check if any version has non-default state
        for( int v = 0; v < pItem->nVersionCount; v++ )
        {
            if( pItem->aVersions[v].nState != 0 )
            {
                hasState = TRUE;
                break;
            }
        }
        
        if( viewed || hasState )
        {
            fprintf( f, "  \"%s\": {\n", appId );
            fprintf( f, "    \"viewed\": %s,\n", viewed ? "true" : "false" );
            fprintf( f, "    \"versions\": {\n" );
            
            // Write version states
            for( int v = 0; v < pItem->nVersionCount; v++ )
            {
                fprintf( f, "      \"%s\": {\"state\": %d",
                        pItem->aVersions[v].szVersion,
                        pItem->aVersions[v].nState );
                
                // Save install path if installed
                if( pItem->aVersions[v].nState == 2 && pItem->aVersions[v].szInstallPath[0] != '\0' )
                {
                    fprintf( f, ", \"path\": \"%s\"", pItem->aVersions[v].szInstallPath );
                }
                
                fprintf( f, "}" );
                
                if( v < pItem->nVersionCount - 1 )
                    fprintf( f, "," );
                fprintf( f, "\n" );
            }
            
            fprintf( f, "    }\n" );
            fprintf( f, "  }" );
            
            if( i < m_nItemCount - 1 )
                fprintf( f, "," );
            fprintf( f, "\n" );
        }
    }
    
    fprintf( f, "}\n" );
    fclose( f );
    
    OutputDebugString( "User state saved\n" );
    return TRUE;
}

//-----------------------------------------------------------------------------
// MergeUserStateWithCatalog - Apply smart logic for NEW badges and updates
//-----------------------------------------------------------------------------
void Store::MergeUserStateWithCatalog()
{
    OutputDebugString( "Merging user state with catalog...\n" );
    
    // Note: We DON'T change states here anymore
    // Update detection will be done at display time by comparing versions
    // This way we don't overwrite the user's installed state (state=2)
    
    OutputDebugString( "Merge complete\n" );
}

//-----------------------------------------------------------------------------
// MarkAppAsViewed - Clear NEW flag and save state
//-----------------------------------------------------------------------------
void Store::MarkAppAsViewed( const char* appId )
{
    // Find app by ID (for now, using name)
    for( int i = 0; i < m_nItemCount; i++ )
    {
        // Use ID for unique identification
		const char* id = m_pItems[i].szID;
		if (!id || id[0] == '\0')
		{
			id = m_pItems[i].szGUID;  // Fallback
			if (!id || id[0] == '\0')
				continue;
		}
        
        if( strcmp( id, appId ) == 0 )
        {
            m_pItems[i].bNew = FALSE;
            SaveUserState( USER_STATE_PATH );
            
            char szDebug[256];
            sprintf( szDebug, "Marked %s as viewed\n", m_pItems[i].szName );
            OutputDebugString( szDebug );
            return;
        }
    }
}

//-----------------------------------------------------------------------------
// SetVersionState - Change version state and save
//-----------------------------------------------------------------------------
void Store::SetVersionState( const char* appId, const char* version, int state )
{
    // Find app
    for( int i = 0; i < m_nItemCount; i++ )
    {
        const char* id = m_pItems[i].szID;
		if (!id || id[0] == '\0')
		{
			id = m_pItems[i].szGUID;  // Fallback
			if (!id || id[0] == '\0')
				continue;
		}
        
        if( strcmp( id, appId ) == 0 )
        {
            // Find version
            for( int v = 0; v < m_pItems[i].nVersionCount; v++ )
            {
                if( strcmp( m_pItems[i].aVersions[v].szVersion, version ) == 0 )
                {
                    m_pItems[i].aVersions[v].nState = state;
                    SaveUserState( USER_STATE_PATH );
                    
                    char szDebug[256];
                    sprintf( szDebug, "Set %s v%s state to %d\n", 
                            m_pItems[i].szName, version, state );
                    OutputDebugString( szDebug );
                    return;
                }
            }
        }
    }
}

BOOL Store::HasUpdateAvailable( StoreItem* pItem )
{
    if( !pItem || pItem->nVersionCount < 2 )
        return FALSE;
    
    // Check if any version is installed
    int installedIndex = -1;
    for( int v = 0; v < pItem->nVersionCount; v++ )
    {
        if( pItem->aVersions[v].nState == 2 )  // INSTALLED
        {
            installedIndex = v;
            break;
        }
    }
    
    if( installedIndex == -1 )
        return FALSE;  // Nothing installed
    
    // If installed version is not the first (latest), update available
    return (installedIndex > 0);
}

//-----------------------------------------------------------------------------
// GetDisplayState - Get state to display (with update detection)
// RULE: Only show UPDATE if user actually has the app installed
//-----------------------------------------------------------------------------
int Store::GetDisplayState( StoreItem* pItem, int versionIndex )
{
    if( !pItem || versionIndex < 0 || versionIndex >= pItem->nVersionCount )
        return 0;
    
    int actualState = pItem->aVersions[versionIndex].nState;
    
    // Card badge (versionIndex == 0): Show best state across ALL versions
    if( versionIndex == 0 )
    {
        // Priority order:
        // 1. Latest version installed -> GREEN
        // 2. Any old version installed -> ORANGE (update available)
        // 3. Any version downloaded -> BLUE
        // 4. Nothing -> GRAY
        
        BOOL hasDownloaded = FALSE;
        int installedIndex = -1;
        
        // Check all versions
        for( int v = 0; v < pItem->nVersionCount; v++ )
        {
            if( pItem->aVersions[v].nState == 2 )  // INSTALLED
            {
                installedIndex = v;
                break;  // Found installed, stop looking
            }
            else if( pItem->aVersions[v].nState == 1 )  // DOWNLOADED
            {
                hasDownloaded = TRUE;
            }
        }
        
        // If something is installed
        if( installedIndex >= 0 )
        {
            // If latest version (index 0) is installed -> GREEN
            if( installedIndex == 0 )
                return 2;  // INSTALLED
            
            // If older version is installed -> ORANGE (update available)
            return 3;  // UPDATE_AVAILABLE
        }
        
        // Nothing installed, but something downloaded -> BLUE
        if( hasDownloaded )
            return 1;  // DOWNLOADED
        
        // Nothing at all -> GRAY
        return 0;  // NOT_DOWNLOADED
    }
    
    // Version list: If this is an old version and it's installed, show UPDATE
    if( actualState == 2 && versionIndex > 0 )
    {
        return 3;  // UPDATE_AVAILABLE
    }
    
    return actualState;
}

//-----------------------------------------------------------------------------
// Initialize
//-----------------------------------------------------------------------------
HRESULT Store::Initialize( LPDIRECT3DDEVICE8 pd3dDevice )
{
    // Detect screen resolution
    DetectResolution( pd3dDevice );
    CalculateLayout();
    
    // Create vertex buffer for rendering quads
    if( FAILED( pd3dDevice->CreateVertexBuffer( 4*sizeof(CUSTOMVERTEX),
                                                 D3DUSAGE_WRITEONLY, 
                                                 D3DFVF_CUSTOMVERTEX,
                                                 D3DPOOL_DEFAULT, 
                                                 &m_pVB ) ) )
    {
        return E_FAIL;
    }

    // Initialize font using bundled font files
    if( FAILED( m_Font.Create( "D:\\Media\\Fonts\\Arial_16.xpr" ) ) )
    {
        OutputDebugString( "Warning: Could not load Arial_16.xpr font file.\n" );
    }

    // Initialize gamepads
    XBInput_CreateGamepads( &m_pGamepads );

    // Load app catalog from JSON file
    if( !LoadCatalogFromFile( CATALOG_PATH ) )
    {
        OutputDebugString( "Failed to load store.json, using fallback data\n" );
        
        // Fallback: Create minimal sample data if JSON fails
        m_nItemCount = 3;
        m_pItems = new StoreItem[m_nItemCount];
        
        int gamesIdx = GetOrCreateCategory("games");
        int mediaIdx = GetOrCreateCategory("media");
        int toolsIdx = GetOrCreateCategory("tools");
        
        sprintf( m_pItems[0].szName, "Doom 64" );
        sprintf( m_pItems[0].szDescription, "Classic FPS" );
        sprintf( m_pItems[0].szAuthor, "id Software" );
        strcpy( m_pItems[0].szGUID, "" );
        m_pItems[0].bNew = FALSE;
        m_pItems[0].nCategoryIndex = gamesIdx;
        m_pItems[0].pIcon = NULL;
        m_pItems[0].nVersionCount = 1;
        m_pItems[0].nSelectedVersion = 0;
        m_pItems[0].nVersionScrollOffset = 0;
        m_pItems[0].bViewingVersionDetail = FALSE;
        strcpy( m_pItems[0].aVersions[0].szVersion, "2.1" );
        strcpy( m_pItems[0].aVersions[0].szChangelog, "Initial release" );
        strcpy( m_pItems[0].aVersions[0].szReleaseDate, "" );
        strcpy( m_pItems[0].aVersions[0].szTitleID, "" );
        strcpy( m_pItems[0].aVersions[0].szRegion, "" );
        m_pItems[0].aVersions[0].szInstallPath[0] = '\0';
        m_pItems[0].aVersions[0].dwSize = 5 * 1024 * 1024;
        m_pItems[0].aVersions[0].nState = 2;
        
        sprintf( m_pItems[1].szName, "XBMC" );
        sprintf( m_pItems[1].szDescription, "Media Center" );
        sprintf( m_pItems[1].szAuthor, "XBMC Team" );
        strcpy( m_pItems[1].szGUID, "" );
        m_pItems[1].bNew = FALSE;
        m_pItems[1].nCategoryIndex = mediaIdx;
        m_pItems[1].pIcon = NULL;
        m_pItems[1].nVersionCount = 1;
        m_pItems[1].nSelectedVersion = 0;
        m_pItems[1].nVersionScrollOffset = 0;
        m_pItems[1].bViewingVersionDetail = FALSE;
        strcpy( m_pItems[1].aVersions[0].szVersion, "3.5" );
        strcpy( m_pItems[1].aVersions[0].szChangelog, "Initial release" );
        strcpy( m_pItems[1].aVersions[0].szReleaseDate, "" );
        strcpy( m_pItems[1].aVersions[0].szTitleID, "" );
        strcpy( m_pItems[1].aVersions[0].szRegion, "" );
        m_pItems[1].aVersions[0].szInstallPath[0] = '\0';
        m_pItems[1].aVersions[0].dwSize = 20 * 1024 * 1024;
        m_pItems[1].aVersions[0].nState = 0;
        
        sprintf( m_pItems[2].szName, "FTP Server" );
        sprintf( m_pItems[2].szDescription, "File Transfer" );
        sprintf( m_pItems[2].szAuthor, "XBDev" );
        strcpy( m_pItems[2].szGUID, "" );
        m_pItems[2].bNew = FALSE;
        m_pItems[2].nCategoryIndex = toolsIdx;
        m_pItems[2].pIcon = NULL;
        m_pItems[2].nVersionCount = 1;
        m_pItems[2].nSelectedVersion = 0;
        m_pItems[2].nVersionScrollOffset = 0;
        m_pItems[2].bViewingVersionDetail = FALSE;
        strcpy( m_pItems[2].aVersions[0].szVersion, "3.2" );
        strcpy( m_pItems[2].aVersions[0].szChangelog, "Initial release" );
        strcpy( m_pItems[2].aVersions[0].szReleaseDate, "" );
        strcpy( m_pItems[2].aVersions[0].szTitleID, "" );
        strcpy( m_pItems[2].aVersions[0].szRegion, "" );
        m_pItems[2].aVersions[0].szInstallPath[0] = '\0';
        m_pItems[2].aVersions[0].dwSize = 512 * 1024;
        m_pItems[2].aVersions[0].nState = 1;
        
        BuildCategoryList();
    }
    
    // Load user state and merge with catalog
    LoadUserState( USER_STATE_PATH );
    MergeUserStateWithCatalog();

    return S_OK;
}

//-----------------------------------------------------------------------------
// Update - Handle input and logic
//-----------------------------------------------------------------------------
void Store::Update()
{
    HandleInput();
}

//-----------------------------------------------------------------------------
// Render
//-----------------------------------------------------------------------------
void Store::Render( LPDIRECT3DDEVICE8 pd3dDevice )
{
    switch( m_CurrentState )
    {
        case UI_MAIN_GRID:
            RenderMainGrid( pd3dDevice );
            break;
            
        case UI_ITEM_DETAILS:
            RenderItemDetails( pd3dDevice );
            break;
            
        case UI_DOWNLOADING:
            RenderDownloading( pd3dDevice );
            break;
            
        case UI_SETTINGS:
            RenderSettings( pd3dDevice );
            break;
    }
}

//-----------------------------------------------------------------------------
// RenderCategorySidebar - Left category menu (dynamic)
//-----------------------------------------------------------------------------
void Store::RenderCategorySidebar( LPDIRECT3DDEVICE8 pd3dDevice )
{
    // Sidebar background
    DrawRect( pd3dDevice, 0, 0, m_fSidebarWidth, m_fScreenHeight, COLOR_SIDEBAR );
    
    // Header with logo area
    DrawText( pd3dDevice, "Homebrew Store", 20.0f, 20.0f, COLOR_WHITE );
    DrawText( pd3dDevice, "for Xbox", 20.0f, 40.0f, COLOR_WHITE );
    
    // Category menu items (dynamic!)
    float itemY = 100.0f;
    float itemH = 50.0f;
    
    for( int i = 0; i < m_nCategoryCount; i++ )
    {
        DWORD bgColor = (i == m_nSelectedCategory) ? COLOR_SIDEBAR_HOVER : COLOR_SIDEBAR;
        
        // Highlight selected category
        if( i == m_nSelectedCategory )
        {
            DrawRect( pd3dDevice, 0, itemY, m_fSidebarWidth, itemH, bgColor );
        }
        
        // Category name with count
        char szText[64];
        sprintf( szText, "%s (%d)", m_aCategories[i].szName, m_aCategories[i].nAppCount );
        DrawText( pd3dDevice, szText, 50.0f, itemY + 15.0f, COLOR_WHITE );
        
        itemY += itemH;
    }
    
    // Hide button at bottom
    DrawText( pd3dDevice, "LB: Hide", 20.0f, m_fScreenHeight - 30.0f, COLOR_WHITE );
}

//-----------------------------------------------------------------------------
// RenderMainGrid - Main store browser (Switch-style with sidebar)
//-----------------------------------------------------------------------------
void Store::RenderMainGrid( LPDIRECT3DDEVICE8 pd3dDevice )
{
    // Draw category sidebar
    RenderCategorySidebar( pd3dDevice );
    
    // Draw title bar (right of sidebar)
    DrawRect( pd3dDevice, m_fSidebarWidth, 0, m_fScreenWidth - m_fSidebarWidth, 60.0f, COLOR_PRIMARY );
    
    // Get current category name
    const char* categoryName = "All Apps";
    if( m_nSelectedCategory >= 0 && m_nSelectedCategory < m_nCategoryCount )
        categoryName = m_aCategories[m_nSelectedCategory].szName;
    
    DrawText( pd3dDevice, categoryName, m_fSidebarWidth + 20.0f, 20.0f, COLOR_WHITE );
    
    // Build filtered indices array for current category
    m_nFilteredCount = 0;
    for( int i = 0; i < m_nItemCount; i++ )
    {
        // "All Apps" (index 0) shows everything
        if( m_nSelectedCategory == 0 || m_pItems[i].nCategoryIndex == m_nSelectedCategory )
        {
            m_pFilteredIndices[m_nFilteredCount++] = i;
        }
    }
    
    // Clamp selection to valid range
    if( m_nSelectedItem >= m_nFilteredCount )
        m_nSelectedItem = m_nFilteredCount > 0 ? m_nFilteredCount - 1 : 0;
    
    // Draw grid of app cards
    int visibleItems = m_nGridRows * m_nGridCols;
    int page = m_nSelectedItem / visibleItems;
    int startIndex = page * visibleItems;
    
    for( int row = 0; row < m_nGridRows; row++ )
    {
        for( int col = 0; col < m_nGridCols; col++ )
        {
            int gridIndex = startIndex + (row * m_nGridCols + col);
            if( gridIndex >= m_nFilteredCount )
                break;
                
            // Get actual item index from filtered array
            int actualItemIndex = m_pFilteredIndices[gridIndex];
            
            float x = m_fGridStartX + col * (m_fCardWidth + 20.0f);
            float y = m_fGridStartY + row * (m_fCardHeight + 20.0f);
            
            BOOL bSelected = (gridIndex == m_nSelectedItem);
            DrawAppCard( pd3dDevice, &m_pItems[actualItemIndex], x, y, bSelected );
        }
    }

    // Draw bottom instructions bar
    DrawRect( pd3dDevice, m_fSidebarWidth, m_fScreenHeight - 50.0f, 
              m_fScreenWidth - m_fSidebarWidth, 50.0f, COLOR_SECONDARY );
    
    char szInstructions[128];
    sprintf( szInstructions, "A: Details  B: Exit  LT/RT: Category  (%d apps)", m_nFilteredCount );
    DrawText( pd3dDevice, szInstructions, m_fSidebarWidth + 20.0f, m_fScreenHeight - 30.0f, COLOR_WHITE );
}

//-----------------------------------------------------------------------------
// RenderItemDetails - Multi-version detail screen with scrollable list
//-----------------------------------------------------------------------------
void Store::RenderItemDetails( LPDIRECT3DDEVICE8 pd3dDevice )
{
    // Get actual item from filtered indices
    if( m_nSelectedItem < 0 || m_nSelectedItem >= m_nFilteredCount )
        return;
        
    int actualItemIndex = m_pFilteredIndices[m_nSelectedItem];
    if( actualItemIndex < 0 || actualItemIndex >= m_nItemCount )
        return;

    StoreItem* pItem = &m_pItems[actualItemIndex];
    
    // Make sure we have at least one version
    if( pItem->nVersionCount == 0 )
        return;
    
    // Clamp selected version
    if( pItem->nSelectedVersion >= pItem->nVersionCount )
        pItem->nSelectedVersion = 0;

    VersionInfo* pCurrentVersion = &pItem->aVersions[pItem->nSelectedVersion];

    // Calculate layout
    float sidebarW = m_fScreenWidth * 0.30f;
    if( sidebarW < 200.0f ) sidebarW = 200.0f;
    if( sidebarW > 280.0f ) sidebarW = 280.0f;
    
    float contentW = m_fScreenWidth - sidebarW;
    float actionBarH = 70.0f;
    
    // MODE 1: Viewing specific version detail (like single app)
    if( pItem->bViewingVersionDetail )
    {
        // Left content area
        DrawRect( pd3dDevice, 0, 0, contentW, m_fScreenHeight, COLOR_BG );
        
        // App title and version
        char szTitle[128];
        sprintf( szTitle, "%s v%s", pItem->szName, pCurrentVersion->szVersion );
        DrawText( pd3dDevice, szTitle, 20.0f, 20.0f, COLOR_WHITE );
        DrawText( pd3dDevice, pItem->szAuthor, 20.0f, 40.0f, COLOR_TEXT_GRAY );
        
        // Screenshot
        float screenshotY = 70.0f;
        float screenshotH = m_fScreenHeight * 0.45f;
        DrawRect( pd3dDevice, 20.0f, screenshotY, contentW - 40.0f, screenshotH, COLOR_CARD_BG );
        
        float descY = screenshotY + screenshotH + 20.0f;
        
        // Description
        DrawText( pd3dDevice, "Description:", 20.0f, descY, COLOR_TEXT_GRAY );
        descY += 25.0f;
        DrawText( pd3dDevice, pItem->szDescription, 20.0f, descY, COLOR_WHITE );
        descY += 50.0f;
        
        // Changelog
        if( pCurrentVersion->szChangelog[0] != '\0' && 
            strcmp( pCurrentVersion->szChangelog, "No changelog available" ) != 0 )
        {
            DrawText( pd3dDevice, "What's New:", 20.0f, descY, COLOR_TEXT_GRAY );
            descY += 25.0f;
            DrawText( pd3dDevice, pCurrentVersion->szChangelog, 20.0f, descY, COLOR_WHITE );
        }
        
        // Right sidebar - Version metadata
        float sidebarX = contentW;
        DrawRect( pd3dDevice, sidebarX, 0, sidebarW, m_fScreenHeight - actionBarH, COLOR_PRIMARY );
        
        float metaY = 20.0f;
        DrawText( pd3dDevice, "Version:", sidebarX + 15.0f, metaY, COLOR_WHITE );
        metaY += 20.0f;
        DrawText( pd3dDevice, pCurrentVersion->szVersion, sidebarX + 15.0f, metaY, COLOR_WHITE );
        metaY += 40.0f;
        
        if( pCurrentVersion->szReleaseDate[0] != '\0' )
        {
            DrawText( pd3dDevice, "Released:", sidebarX + 15.0f, metaY, COLOR_WHITE );
            metaY += 20.0f;
            DrawText( pd3dDevice, pCurrentVersion->szReleaseDate, sidebarX + 15.0f, metaY, COLOR_WHITE );
            metaY += 40.0f;
        }
        
        char szTemp[128];
        DrawText( pd3dDevice, "Size:", sidebarX + 15.0f, metaY, COLOR_WHITE );
        metaY += 20.0f;
        sprintf( szTemp, "%.1f MB", pCurrentVersion->dwSize / (1024.0f * 1024.0f) );
        DrawText( pd3dDevice, szTemp, sidebarX + 15.0f, metaY, COLOR_WHITE );
        metaY += 40.0f;
        
        // Title ID
        if( pCurrentVersion->szTitleID[0] != '\0' )
        {
            DrawText( pd3dDevice, "Title ID:", sidebarX + 15.0f, metaY, COLOR_WHITE );
            metaY += 20.0f;
            DrawText( pd3dDevice, pCurrentVersion->szTitleID, sidebarX + 15.0f, metaY, COLOR_WHITE );
            metaY += 40.0f;
        }
        
        // Region
        if( pCurrentVersion->szRegion[0] != '\0' )
        {
            DrawText( pd3dDevice, "Region:", sidebarX + 15.0f, metaY, COLOR_WHITE );
            metaY += 20.0f;
            DrawText( pd3dDevice, pCurrentVersion->szRegion, sidebarX + 15.0f, metaY, COLOR_WHITE );
            metaY += 40.0f;
        }
        
        // Status - use display state for update detection
        int displayState = GetDisplayState( pItem, pItem->nSelectedVersion );
        const char* statusText;
        DWORD statusColor;
        switch( displayState )
        {
            case 2: statusText = "INSTALLED"; statusColor = COLOR_SUCCESS; break;
            case 1: statusText = "DOWNLOADED"; statusColor = COLOR_DOWNLOAD; break;
            case 3: statusText = "UPDATE"; statusColor = 0xFFFF9800; break;
            default: statusText = "NOT DOWNLOADED"; statusColor = COLOR_TEXT_GRAY; break;
        }
        DrawText( pd3dDevice, "Status:", sidebarX + 15.0f, metaY, COLOR_WHITE );
        metaY += 20.0f;
        DrawText( pd3dDevice, statusText, sidebarX + 15.0f, metaY, statusColor );
        metaY += 40.0f;
        
        // Show install path if installed
        if( pCurrentVersion->nState == 2 && pCurrentVersion->szInstallPath[0] != '\0' )
        {
            DrawText( pd3dDevice, "Installed:", sidebarX + 15.0f, metaY, COLOR_WHITE );
            metaY += 20.0f;
            
            // Wrap path if too long - show last part
            const char* pathToShow = pCurrentVersion->szInstallPath;
            if( strlen( pathToShow ) > 20 )
            {
                // Show "E:\Apps\..." format
                char shortPath[64];
                sprintf( shortPath, "...%s", pathToShow + strlen(pathToShow) - 15 );
                DrawText( pd3dDevice, shortPath, sidebarX + 15.0f, metaY, COLOR_TEXT_GRAY );
            }
            else
            {
                DrawText( pd3dDevice, pathToShow, sidebarX + 15.0f, metaY, COLOR_TEXT_GRAY );
            }
            metaY += 40.0f;
        }
        
        // Action buttons - use ACTUAL state for buttons, not display state
        float actionBarY = m_fScreenHeight - actionBarH;
        DrawRect( pd3dDevice, 0, actionBarY, m_fScreenWidth, actionBarH, COLOR_SECONDARY );
        
        float btnY = actionBarY + 15.0f;
        float btnH = 40.0f;
        float btnSpacing = 20.0f;
        float btnStartX = 40.0f;
        
        // Use actual state for buttons (not displayState which shows UPDATE)
        int actualState = pCurrentVersion->nState;
        
        switch( actualState )
        {
            case 0: // NOT_DOWNLOADED
            {
                float btnW = (m_fScreenWidth - btnStartX * 2 - btnSpacing) / 2.0f;
                DrawRect( pd3dDevice, btnStartX, btnY, btnW, btnH, COLOR_DOWNLOAD );
                DrawText( pd3dDevice, "(A) Download", btnStartX + 20.0f, btnY + 12.0f, COLOR_WHITE );
                DrawRect( pd3dDevice, btnStartX + btnW + btnSpacing, btnY, btnW, btnH, COLOR_CARD_BG );
                DrawText( pd3dDevice, "(B) Back", btnStartX + btnW + btnSpacing + 40.0f, btnY + 12.0f, COLOR_WHITE );
                break;
            }
            case 1: // DOWNLOADED
            {
                float btnW = (m_fScreenWidth - btnStartX * 2 - btnSpacing * 2) / 3.0f;
                DrawRect( pd3dDevice, btnStartX, btnY, btnW, btnH, COLOR_SUCCESS );
                DrawText( pd3dDevice, "(A) Install", btnStartX + 15.0f, btnY + 12.0f, COLOR_WHITE );
                DrawRect( pd3dDevice, btnStartX + btnW + btnSpacing, btnY, btnW, btnH, 0xFF9E9E9E );
                DrawText( pd3dDevice, "(X) Delete", btnStartX + btnW + btnSpacing + 15.0f, btnY + 12.0f, COLOR_WHITE );
                DrawRect( pd3dDevice, btnStartX + (btnW + btnSpacing) * 2, btnY, btnW, btnH, COLOR_CARD_BG );
                DrawText( pd3dDevice, "(B) Back", btnStartX + (btnW + btnSpacing) * 2 + 20.0f, btnY + 12.0f, COLOR_WHITE );
                break;
            }
            case 2: // INSTALLED
            {
                // If this is an old version (displayState shows UPDATE), show Launch + Uninstall
                // If this is latest version, show Launch + Uninstall
                float btnW = (m_fScreenWidth - btnStartX * 2 - btnSpacing * 2) / 3.0f;
                DrawRect( pd3dDevice, btnStartX, btnY, btnW, btnH, COLOR_SUCCESS );
                DrawText( pd3dDevice, "(A) Launch", btnStartX + 15.0f, btnY + 12.0f, COLOR_WHITE );
                DrawRect( pd3dDevice, btnStartX + btnW + btnSpacing, btnY, btnW, btnH, 0xFFD32F2F );
                DrawText( pd3dDevice, "(X) Uninstall", btnStartX + btnW + btnSpacing + 8.0f, btnY + 12.0f, COLOR_WHITE );
                DrawRect( pd3dDevice, btnStartX + (btnW + btnSpacing) * 2, btnY, btnW, btnH, COLOR_CARD_BG );
                DrawText( pd3dDevice, "(B) Back", btnStartX + (btnW + btnSpacing) * 2 + 20.0f, btnY + 12.0f, COLOR_WHITE );
                break;
            }
        }
        
        return; // Done with single version detail view
    }
    
    // MODE 2: Version list view (for multiple versions)
    DrawRect( pd3dDevice, 0, 0, contentW, m_fScreenHeight, COLOR_BG );
    
    DrawText( pd3dDevice, pItem->szName, 20.0f, 20.0f, COLOR_WHITE );
    DrawText( pd3dDevice, pItem->szAuthor, 20.0f, 40.0f, COLOR_TEXT_GRAY );
    
    float screenshotY = 70.0f;
    float screenshotH = m_fScreenHeight * 0.30f;
    DrawRect( pd3dDevice, 20.0f, screenshotY, contentW - 40.0f, screenshotH, COLOR_CARD_BG );
    
    float contentY = screenshotY + screenshotH + 20.0f;
    
    DrawText( pd3dDevice, "Description:", 20.0f, contentY, COLOR_TEXT_GRAY );
    contentY += 25.0f;
    DrawText( pd3dDevice, pItem->szDescription, 20.0f, contentY, COLOR_WHITE );
    contentY += 50.0f;
    
    // VERSION LIST with scrolling
    if( pItem->nVersionCount > 1 )
    {
        DrawText( pd3dDevice, "Available Versions (UP/DOWN to browse, A to view):", 20.0f, contentY, COLOR_TEXT_GRAY );
        contentY += 30.0f;
        
        // Calculate visible items (max 3-4 depending on space)
        int maxVisible = 3;
        
        // Adjust scroll offset
        if( pItem->nSelectedVersion < pItem->nVersionScrollOffset )
            pItem->nVersionScrollOffset = pItem->nSelectedVersion;
        if( pItem->nSelectedVersion >= pItem->nVersionScrollOffset + maxVisible )
            pItem->nVersionScrollOffset = pItem->nSelectedVersion - maxVisible + 1;
        
        // Show "More above" indicator BEFORE the list
        if( pItem->nVersionScrollOffset > 0 )
        {
            DrawText( pd3dDevice, "^ More above", 30.0f, contentY, COLOR_TEXT_GRAY );
            contentY += 20.0f;
        }
        
        float listStartY = contentY;
        
        // Draw visible versions
        for( int i = 0; i < maxVisible && (i + pItem->nVersionScrollOffset) < pItem->nVersionCount; i++ )
        {
            int vIdx = i + pItem->nVersionScrollOffset;
            VersionInfo* pVer = &pItem->aVersions[vIdx];
            BOOL bSelected = (vIdx == pItem->nSelectedVersion);
            
            float itemH = 55.0f;
            DWORD bgColor = bSelected ? COLOR_PRIMARY : COLOR_CARD_BG;
            
            DrawRect( pd3dDevice, 20.0f, contentY, contentW - 40.0f, itemH, bgColor );
            
            // Version number
            char szVer[64];
            sprintf( szVer, "v%s", pVer->szVersion );
            DrawText( pd3dDevice, szVer, 30.0f, contentY + 8.0f, COLOR_WHITE );
            
            // Date if available
            if( pVer->szReleaseDate[0] != '\0' )
            {
                DrawText( pd3dDevice, pVer->szReleaseDate, 30.0f, contentY + 28.0f, COLOR_TEXT_GRAY );
            }
            
            // Size on the right
            char szSize[64];
            sprintf( szSize, "%.1f MB", pVer->dwSize / (1024.0f * 1024.0f) );
            DrawText( pd3dDevice, szSize, contentW - 120.0f, contentY + 18.0f, COLOR_WHITE );
            
            // State badge - right aligned, next to size
            if( pVer->nState == 2 ) // INSTALLED
            {
                DrawText( pd3dDevice, "INSTALLED", contentW - 220.0f, contentY + 18.0f, COLOR_SUCCESS );
            }
            else if( pVer->nState == 1 ) // DOWNLOADED
            {
                DrawText( pd3dDevice, "DOWNLOADED", contentW - 240.0f, contentY + 18.0f, COLOR_DOWNLOAD );
            }
            
            contentY += itemH + 5.0f;
        }
        
        // Show "More below" indicator AFTER the list
        if( pItem->nVersionScrollOffset + maxVisible < pItem->nVersionCount )
        {
            DrawText( pd3dDevice, "v More below", 30.0f, contentY + 5.0f, COLOR_TEXT_GRAY );
        }
    }
    
    // Right sidebar
    float sidebarX = contentW;
    DrawRect( pd3dDevice, sidebarX, 0, sidebarW, m_fScreenHeight - actionBarH, COLOR_PRIMARY );
    
    float metaY = 20.0f;
    DrawText( pd3dDevice, "Title:", sidebarX + 15.0f, metaY, COLOR_WHITE );
    metaY += 20.0f;
    DrawText( pd3dDevice, pItem->szName, sidebarX + 15.0f, metaY, COLOR_WHITE );
    metaY += 40.0f;
    
    DrawText( pd3dDevice, "Author:", sidebarX + 15.0f, metaY, COLOR_WHITE );
    metaY += 20.0f;
    DrawText( pd3dDevice, pItem->szAuthor, sidebarX + 15.0f, metaY, COLOR_WHITE );
    metaY += 40.0f;
    
    char szTemp[128];
    sprintf( szTemp, "%d version(s)", pItem->nVersionCount );
    DrawText( pd3dDevice, szTemp, sidebarX + 15.0f, metaY, COLOR_TEXT_GRAY );
    
    // Bottom bar
    float actionBarY = m_fScreenHeight - actionBarH;
    DrawRect( pd3dDevice, 0, actionBarY, m_fScreenWidth, actionBarH, COLOR_SECONDARY );
    DrawText( pd3dDevice, "(A) View Version  (B) Back to Grid", 40.0f, actionBarY + 25.0f, COLOR_WHITE );
}

//-----------------------------------------------------------------------------
// RenderDownloading - Download progress screen (Switch-style)
//-----------------------------------------------------------------------------
void Store::RenderDownloading( LPDIRECT3DDEVICE8 pd3dDevice )
{
    // Title bar
    DrawRect( pd3dDevice, 0, 0, m_fScreenWidth, 60.0f, COLOR_PRIMARY );
    DrawText( pd3dDevice, "DOWNLOADING", 20.0f, 20.0f, COLOR_WHITE );

    // Center panel
    float panelW = m_fScreenWidth * 0.7f;
    if( panelW > 500.0f ) panelW = 500.0f;
    float panelH = 200.0f;
    float panelX = (m_fScreenWidth - panelW) / 2.0f;
    float panelY = (m_fScreenHeight - panelH) / 2.0f;
    
    DrawRect( pd3dDevice, panelX, panelY, panelW, panelH, COLOR_CARD_BG );
    
    // App name from filtered index
    if( m_nSelectedItem >= 0 && m_nSelectedItem < m_nFilteredCount )
    {
        int actualItemIndex = m_pFilteredIndices[m_nSelectedItem];
        if( actualItemIndex >= 0 && actualItemIndex < m_nItemCount )
        {
            DrawText( pd3dDevice, m_pItems[actualItemIndex].szName, panelX + 20.0f, panelY + 20.0f, COLOR_WHITE );
        }
    }
    
    // Progress bar
    float barY = panelY + 80.0f;
    float barW = panelW - 40.0f;
    DrawRect( pd3dDevice, panelX + 20.0f, barY, barW, 30.0f, COLOR_SECONDARY );
    DrawRect( pd3dDevice, panelX + 22.0f, barY + 2.0f, (barW - 4.0f) * 0.5f, 26.0f, COLOR_DOWNLOAD );
    
    // Progress text
    DrawText( pd3dDevice, "50% Complete", panelX + 20.0f, barY + 50.0f, COLOR_WHITE );
    DrawText( pd3dDevice, "2048 KB / 4096 KB", panelX + 20.0f, barY + 70.0f, COLOR_TEXT_GRAY );
}

//-----------------------------------------------------------------------------
// RenderSettings - Settings screen (Switch-style)
//-----------------------------------------------------------------------------
void Store::RenderSettings( LPDIRECT3DDEVICE8 pd3dDevice )
{
    // Title bar
    DrawRect( pd3dDevice, 0, 0, m_fScreenWidth, 60.0f, COLOR_PRIMARY );
    DrawText( pd3dDevice, "SETTINGS", 20.0f, 20.0f, COLOR_WHITE );

    // Menu items
    float itemY = 100.0f;
    float itemH = 50.0f;
    float itemW = m_fScreenWidth - 80.0f;
    
    // Network Settings
    DrawRect( pd3dDevice, 40.0f, itemY, itemW, itemH, COLOR_CARD_BG );
    DrawText( pd3dDevice, "Network Settings", 60.0f, itemY + 15.0f, COLOR_WHITE );
    itemY += itemH + 10.0f;
    
    // Storage Management
    DrawRect( pd3dDevice, 40.0f, itemY, itemW, itemH, COLOR_CARD_BG );
    DrawText( pd3dDevice, "Storage Management", 60.0f, itemY + 15.0f, COLOR_WHITE );
    itemY += itemH + 10.0f;
    
    // About
    DrawRect( pd3dDevice, 40.0f, itemY, itemW, itemH, COLOR_CARD_BG );
    DrawText( pd3dDevice, "About", 60.0f, itemY + 15.0f, COLOR_WHITE );

    // Bottom bar
    DrawRect( pd3dDevice, 0, m_fScreenHeight - 50.0f, m_fScreenWidth, 50.0f, COLOR_SECONDARY );
    DrawText( pd3dDevice, "B: Back", 20.0f, m_fScreenHeight - 30.0f, COLOR_WHITE );
}

//-----------------------------------------------------------------------------
// DetectResolution - Get the current display mode resolution
//-----------------------------------------------------------------------------
void Store::DetectResolution( LPDIRECT3DDEVICE8 pd3dDevice )
{
    D3DPRESENT_PARAMETERS pp;
    ZeroMemory( &pp, sizeof(pp) );
    
    // Get current backbuffer dimensions
    LPDIRECT3DSURFACE8 pBackBuffer = NULL;
    if( SUCCEEDED( pd3dDevice->GetBackBuffer( 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer ) ) )
    {
        D3DSURFACE_DESC desc;
        if( SUCCEEDED( pBackBuffer->GetDesc( &desc ) ) )
        {
            m_fScreenWidth = (float)desc.Width;
            m_fScreenHeight = (float)desc.Height;
            
            char szDebug[256];
            sprintf( szDebug, "Detected resolution: %.0fx%.0f\n", m_fScreenWidth, m_fScreenHeight );
            OutputDebugString( szDebug );
        }
        pBackBuffer->Release();
    }
    
    // Fallback to common Xbox resolutions if detection failed
    if( m_fScreenWidth < 100.0f )
    {
        m_fScreenWidth = 640.0f;
        m_fScreenHeight = 480.0f;
        OutputDebugString( "Using default 640x480 resolution\n" );
    }
}

//-----------------------------------------------------------------------------
// CalculateLayout - Calculate dynamic layout based on resolution
//-----------------------------------------------------------------------------
void Store::CalculateLayout()
{
    // Sidebar width scales with resolution
    m_fSidebarWidth = m_fScreenWidth * 0.25f;  // 25% of screen width
    if( m_fSidebarWidth < 180.0f ) m_fSidebarWidth = 180.0f;
    if( m_fSidebarWidth > 250.0f ) m_fSidebarWidth = 250.0f;
    
    // Calculate grid dimensions
    float contentWidth = m_fScreenWidth - m_fSidebarWidth - 80.0f; // Minus margins
    float contentHeight = m_fScreenHeight - 170.0f; // Minus header and footer
    
    // Determine grid layout based on resolution
    if( m_fScreenWidth >= 1280.0f )
    {
        // HD resolutions: 4x2 grid
        m_nGridCols = 4;
        m_nGridRows = 2;
    }
    else if( m_fScreenWidth >= 1024.0f )
    {
        // 1024x768: 3x2 grid
        m_nGridCols = 3;
        m_nGridRows = 2;
    }
    else if( m_fScreenWidth >= 720.0f )
    {
        // 720p: 3x2 grid
        m_nGridCols = 3;
        m_nGridRows = 2;
    }
    else
    {
        // 480p/640x480: 2x2 grid
        m_nGridCols = 2;
        m_nGridRows = 2;
    }
    
    // Calculate card size based on available space
    float cardSpacing = 20.0f;
    m_fCardWidth = (contentWidth - (m_nGridCols + 1) * cardSpacing) / m_nGridCols;
    m_fCardHeight = (contentHeight - (m_nGridRows + 1) * cardSpacing) / m_nGridRows;
    
    // Set grid starting position
    m_fGridStartX = m_fSidebarWidth + 40.0f;
    m_fGridStartY = 100.0f;
    
    char szDebug[256];
    sprintf( szDebug, "Layout: %dx%d grid, cards %.0fx%.0f, sidebar %.0f\n", 
             m_nGridCols, m_nGridRows, m_fCardWidth, m_fCardHeight, m_fSidebarWidth );
    OutputDebugString( szDebug );
}

//-----------------------------------------------------------------------------
// DrawRect - Draw a filled rectangle
//-----------------------------------------------------------------------------
void Store::DrawRect( LPDIRECT3DDEVICE8 pd3dDevice, float x, float y, float w, float h, DWORD color )
{
    CUSTOMVERTEX vertices[] =
    {
        { x,     y,     0.5f, 1.0f, color },
        { x + w, y,     0.5f, 1.0f, color },
        { x,     y + h, 0.5f, 1.0f, color },
        { x + w, y + h, 0.5f, 1.0f, color },
    };

    VOID* pVertices;
    if( FAILED( m_pVB->Lock( 0, sizeof(vertices), (BYTE**)&pVertices, 0 ) ) )
        return;
    memcpy( pVertices, vertices, sizeof(vertices) );
    m_pVB->Unlock();

    pd3dDevice->SetStreamSource( 0, m_pVB, sizeof(CUSTOMVERTEX) );
    pd3dDevice->SetVertexShader( D3DFVF_CUSTOMVERTEX );
    pd3dDevice->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 2 );
}

//-----------------------------------------------------------------------------
// DrawText - Draw text using CXBFont
//-----------------------------------------------------------------------------
void Store::DrawText( LPDIRECT3DDEVICE8 pd3dDevice, const char* text, float x, float y, DWORD color )
{
    // Convert to wide string for CXBFont
    WCHAR wstr[256];
    MultiByteToWideChar( CP_ACP, 0, text, -1, wstr, 256 );
    
    m_Font.DrawText( x, y, color, wstr, XBFONT_LEFT );
}

//-----------------------------------------------------------------------------
// DrawAppCard - Draw a single app card (Switch-style, dynamic size)
//-----------------------------------------------------------------------------
void Store::DrawAppCard( LPDIRECT3DDEVICE8 pd3dDevice, StoreItem* pItem, float x, float y, BOOL bSelected )
{
    // Card background - normal color always
    DWORD cardColor = bSelected ? COLOR_SECONDARY : COLOR_CARD_BG;
    DrawRect( pd3dDevice, x, y, m_fCardWidth, m_fCardHeight, cardColor );
    
    // Selection highlight border
    if( bSelected )
    {
        DrawRect( pd3dDevice, x - 3, y - 3, m_fCardWidth + 6, m_fCardHeight + 6, COLOR_PRIMARY );
        DrawRect( pd3dDevice, x, y, m_fCardWidth, m_fCardHeight, cardColor );
    }
    
    // App icon/thumbnail area (square, as large as possible)
    float thumbSize = m_fCardWidth - 20.0f;
    if( thumbSize > m_fCardHeight - 60.0f ) // Leave room for text
        thumbSize = m_fCardHeight - 60.0f;
    
    DrawRect( pd3dDevice, x + 10, y + 10, thumbSize, thumbSize, COLOR_BG );
    
    // Status badge (top-right corner) - RED if new, otherwise varies by state
    DWORD badgeColor;
    
    if( pItem->bNew )
    {
        // NEW items get bright red badge
        badgeColor = COLOR_NEW;
    }
    else
    {
        // Normal state colors - use display state for update detection
        int state = (pItem->nVersionCount > 0) ? GetDisplayState( pItem, 0 ) : 0;
        
        switch( state )
        {
            case 2: // STATE_INSTALLED
                badgeColor = COLOR_SUCCESS; // Green checkmark
                break;
            case 1: // STATE_DOWNLOADED
                badgeColor = COLOR_DOWNLOAD; // Blue - ready to install
                break;
            case 3: // STATE_UPDATE_AVAILABLE
                badgeColor = 0xFFFF9800; // Orange - update available
                break;
            default: // STATE_NOT_DOWNLOADED
                badgeColor = COLOR_TEXT_GRAY; // Gray - not downloaded
                break;
        }
    }
    DrawRect( pd3dDevice, x + m_fCardWidth - 35, y + 10, 25, 25, badgeColor );

    // App name below thumbnail
    DrawText( pd3dDevice, pItem->szName, x + 10, y + thumbSize + 15, COLOR_WHITE );
    
    // Author name (only if there's room)
    if( m_fCardHeight > 180.0f )
    {
        DrawText( pd3dDevice, pItem->szAuthor, x + 10, y + thumbSize + 35, COLOR_TEXT_GRAY );
    }
}

//-----------------------------------------------------------------------------
// HandleInput - Process controller input
//-----------------------------------------------------------------------------
void Store::HandleInput()
{
    // Get input from all gamepads
    XBInput_GetInput( m_pGamepads );
    
    // Use gamepad 0
    XBGAMEPAD* pGamepad = &m_pGamepads[0];
    
    // Get pressed buttons (buttons that just went down this frame)
    WORD wPressed = pGamepad->wPressedButtons;

    if( m_CurrentState == UI_MAIN_GRID )
    {
        // Left trigger - previous category
        if( pGamepad->bPressedAnalogButtons[XINPUT_GAMEPAD_LEFT_TRIGGER] )
        {
            m_nSelectedCategory--;
            if( m_nSelectedCategory < 0 ) m_nSelectedCategory = m_nCategoryCount - 1;
            m_nSelectedItem = 0; // Reset selection
        }
        
        // Right trigger - next category
        if( pGamepad->bPressedAnalogButtons[XINPUT_GAMEPAD_RIGHT_TRIGGER] )
        {
            m_nSelectedCategory++;
            if( m_nSelectedCategory >= m_nCategoryCount ) m_nSelectedCategory = 0;
            m_nSelectedItem = 0; // Reset selection
        }
        
        // Navigate grid with D-pad
        if( wPressed & XINPUT_GAMEPAD_DPAD_LEFT )
        {
            if( m_nSelectedItem % m_nGridCols > 0 )
                m_nSelectedItem--;
        }
        if( wPressed & XINPUT_GAMEPAD_DPAD_RIGHT )
        {
            if( m_nSelectedItem % m_nGridCols < m_nGridCols - 1 )
                m_nSelectedItem++;
        }
        if( wPressed & XINPUT_GAMEPAD_DPAD_UP )
        {
            if( m_nSelectedItem >= m_nGridCols )
                m_nSelectedItem -= m_nGridCols;
        }
        if( wPressed & XINPUT_GAMEPAD_DPAD_DOWN )
        {
            m_nSelectedItem += m_nGridCols;
            // Clamp to valid range based on filtered items
            // (simplified - you'd want to count filtered items properly)
        }

        // A button - select item
        if( pGamepad->bPressedAnalogButtons[XINPUT_GAMEPAD_A] )
        {
            // For single-version apps, go straight to detail view
            if( m_nSelectedItem >= 0 && m_nSelectedItem < m_nFilteredCount )
            {
                int actualItemIndex = m_pFilteredIndices[m_nSelectedItem];
                if( actualItemIndex >= 0 && actualItemIndex < m_nItemCount )
                {
                    // Clear NEW flag when user views the app
                    m_pItems[actualItemIndex].bNew = FALSE;
                    
                    // Save user state (app viewed)
                    SaveUserState( USER_STATE_PATH );
                    
                    if( m_pItems[actualItemIndex].nVersionCount == 1 )
                        m_pItems[actualItemIndex].bViewingVersionDetail = TRUE;
                    else
                        m_pItems[actualItemIndex].bViewingVersionDetail = FALSE;
                }
            }
            m_CurrentState = UI_ITEM_DETAILS;
        }

        // Y button - settings
        if( pGamepad->bPressedAnalogButtons[XINPUT_GAMEPAD_Y] )
        {
            m_CurrentState = UI_SETTINGS;
        }
    }
    else if( m_CurrentState == UI_ITEM_DETAILS )
    {
        // Get actual item index
        if( m_nSelectedItem >= 0 && m_nSelectedItem < m_nFilteredCount )
        {
            int actualItemIndex = m_pFilteredIndices[m_nSelectedItem];
            StoreItem* pItem = &m_pItems[actualItemIndex];
            
            // B button - back
            if( pGamepad->bPressedAnalogButtons[XINPUT_GAMEPAD_B] )
            {
                if( pItem->bViewingVersionDetail )
                {
                    // If single version, go back to grid
                    // If multi version, go back to version list
                    if( pItem->nVersionCount == 1 )
                    {
                        m_CurrentState = UI_MAIN_GRID;
                    }
                    else
                    {
                        // Back to version list
                        pItem->bViewingVersionDetail = FALSE;
                    }
                }
                else
                {
                    // Back to grid
                    m_CurrentState = UI_MAIN_GRID;
                }
            }
            
            // If viewing version detail, handle actions for that specific version
            if( pItem->bViewingVersionDetail )
            {
                VersionInfo* pVer = &pItem->aVersions[pItem->nSelectedVersion];
                
                // A button - download/install/launch
                if( pGamepad->bPressedAnalogButtons[XINPUT_GAMEPAD_A] )
                {
                    switch( pVer->nState )
                    {
                        case 0: m_CurrentState = UI_DOWNLOADING; break; // Download
                        case 1:  // Install (this IS the update if newer version exists!)
                        {
                            // Generate install path: E:\Apps\AppName_Version
                            char appName[64];
                            // Remove spaces and special chars from app name
                            int j = 0;
                            for( const char* s = pItem->szName; *s && j < 63; s++ )
                            {
                                if( (*s >= 'A' && *s <= 'Z') || (*s >= 'a' && *s <= 'z') || (*s >= '0' && *s <= '9') )
                                    appName[j++] = *s;
                            }
                            appName[j] = '\0';
                            
                            // Remove dots from version for path
                            char verClean[16];
                            j = 0;
                            for( const char* s = pVer->szVersion; *s && j < 15; s++ )
                            {
                                if( *s != '.' && *s != ' ' )
                                    verClean[j++] = *s;
                            }
                            verClean[j] = '\0';
                            
                            // Create install path
                            sprintf( pVer->szInstallPath, "E:\\Apps\\%s_%s", appName, verClean );
                            
                            // Mark as installed
                            pVer->nState = 2;
                            SaveUserState( USER_STATE_PATH );
                            
                            char szDebug[256];
                            sprintf( szDebug, "Installed to: %s\n", pVer->szInstallPath );
                            OutputDebugString( szDebug );
                            break;
                        }
                        case 2: /* Launch */ break;
                    }
                }
                
                // X button - delete/uninstall
                if( pGamepad->bPressedAnalogButtons[XINPUT_GAMEPAD_X] )
                {
                    if( pVer->nState == 1 || pVer->nState == 2 )
                    {
                        // TODO: Actually delete folder at pVer->szInstallPath
                        pVer->szInstallPath[0] = '\0';  // Clear install path
                        pVer->nState = 0;
                        SaveUserState( USER_STATE_PATH );
                        
                        OutputDebugString( "Uninstalled and deleted folder\n" );
                    }
                }
            }
            // If viewing version list
            else if( pItem->nVersionCount > 1 )
            {
                // Up/Down - navigate version list
                if( wPressed & XINPUT_GAMEPAD_DPAD_UP )
                {
                    pItem->nSelectedVersion--;
                    if( pItem->nSelectedVersion < 0 )
                        pItem->nSelectedVersion = pItem->nVersionCount - 1;
                }
                if( wPressed & XINPUT_GAMEPAD_DPAD_DOWN )
                {
                    pItem->nSelectedVersion++;
                    if( pItem->nSelectedVersion >= pItem->nVersionCount )
                        pItem->nSelectedVersion = 0;
                }
                
                // A button - view selected version details
                if( pGamepad->bPressedAnalogButtons[XINPUT_GAMEPAD_A] )
                {
                    pItem->bViewingVersionDetail = TRUE;
                }
            }
            // Single version - act immediately
            else
            {
                VersionInfo* pVer = &pItem->aVersions[0];
                
                if( pGamepad->bPressedAnalogButtons[XINPUT_GAMEPAD_A] )
                {
                    switch( pVer->nState )
                    {
                        case 0: m_CurrentState = UI_DOWNLOADING; break;
                        case 1:  // Install
                        {
                            // Generate install path
                            char appName[64];
                            int j = 0;
                            for( const char* s = pItem->szName; *s && j < 63; s++ )
                            {
                                if( (*s >= 'A' && *s <= 'Z') || (*s >= 'a' && *s <= 'z') || (*s >= '0' && *s <= '9') )
                                    appName[j++] = *s;
                            }
                            appName[j] = '\0';
                            
                            char verClean[16];
                            j = 0;
                            for( const char* s = pVer->szVersion; *s && j < 15; s++ )
                            {
                                if( *s != '.' && *s != ' ' )
                                    verClean[j++] = *s;
                            }
                            verClean[j] = '\0';
                            
                            sprintf( pVer->szInstallPath, "E:\\Apps\\%s_%s", appName, verClean );
                            pVer->nState = 2;
                            SaveUserState( USER_STATE_PATH );
                            break;
                        }
                        case 2: /* Launch */ break;
                    }
                }
                
                if( pGamepad->bPressedAnalogButtons[XINPUT_GAMEPAD_X] )
                {
                    if( pVer->nState == 1 || pVer->nState == 2 )
                    {
                        pVer->szInstallPath[0] = '\0';
                        pVer->nState = 0;
                        SaveUserState( USER_STATE_PATH );
                    }
                }
            }
        }
    }
    else if( m_CurrentState == UI_DOWNLOADING )
    {
        // Simulate download completion (in real app, this would be async)
        // For now, B button completes download
        if( pGamepad->bPressedAnalogButtons[XINPUT_GAMEPAD_B] )
        {
            // Get currently selected item
            if( m_nSelectedItem >= 0 && m_nSelectedItem < m_nFilteredCount )
            {
                int actualItemIndex = m_pFilteredIndices[m_nSelectedItem];
                if( actualItemIndex >= 0 && actualItemIndex < m_nItemCount )
                {
                    StoreItem* pItem = &m_pItems[actualItemIndex];
                    
                    // Mark selected version as downloaded
                    if( pItem->nSelectedVersion >= 0 && pItem->nSelectedVersion < pItem->nVersionCount )
                    {
                        pItem->aVersions[pItem->nSelectedVersion].nState = 1;  // DOWNLOADED
                        SaveUserState( USER_STATE_PATH );
                        
                        OutputDebugString( "Download complete - marked as DOWNLOADED\n" );
                    }
                }
            }
            
            // Return to details
            m_CurrentState = UI_ITEM_DETAILS;
        }
    }
    else if( m_CurrentState == UI_SETTINGS )
    {
        // B button - back
        if( pGamepad->bPressedAnalogButtons[XINPUT_GAMEPAD_B] )
        {
            m_CurrentState = UI_MAIN_GRID;
        }
    }
}