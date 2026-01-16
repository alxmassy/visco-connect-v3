# WireGuard IP Deallocation on Logout - Implementation Summary

## Problem Statement
When users logged out from the camera server application, their allocated WireGuard IP addresses remained in the backend database, causing issues when the same or different users tried to rejoin the VPN network. The application was not properly deallocating WireGuard resources during logout.

## Solution Overview
Implemented a comprehensive logout flow that ensures WireGuard IP addresses are properly deallocated from the backend database when users log out from the application (not just when they disconnect from the VPN network).

## Changes Made

### 1. Backend API Changes (`/backend/app/routers/auth_routes.py`)

**Enhanced `/logout` endpoint** to include WireGuard configuration cleanup:

- **Before**: Only invalidated user session in database
- **After**: 
  - Invalidates user session
  - Automatically revokes user's WireGuard configuration
  - Removes peer from server WireGuard config file
  - Frees the allocated IP address in database
  - Provides detailed response about WireGuard cleanup status

**Key Implementation Details**:
```python
# Import WireGuard service
from ..services.wireguard_service import WireGuardService
from ..utils.system_utils import remove_peer_from_wg_config

# Get user's WireGuard config
wg_service = WireGuardService()
wg_config = wg_service.get_user_config(db, current_user)

if wg_config:
    # Remove from server config file
    remove_peer_from_wg_config(wg_config.public_key)
    
    # Delete from database (frees IP)
    wg_service.revoke_config(db, current_user)
```

### 2. Qt Application Changes

#### A. UserProfileWidget Header (`include/UserProfileWidget.h`)
- Added new slots: `onLogoutFinished()`, `onLogoutError()`
- Added new methods: `performLogoutApiCall()`, `completeLogoutProcess()`
- Added member variable: `QNetworkReply *m_logoutReply`

#### B. UserProfileWidget Implementation (`src/UserProfileWidget.cpp`)
**Complete logout flow rewrite**:

1. **User clicks logout** → Show confirmation dialog
2. **User confirms** → Call backend logout API
3. **API call successful** → Backend revokes WireGuard IP and session
4. **Complete local cleanup** → Clear tokens, restart application

**Key Features**:
- **API-first approach**: Always attempts to call logout endpoint first
- **Graceful fallback**: Continues with local logout even if API fails
- **User feedback**: Shows "Logging out..." status during API call
- **Comprehensive logging**: Tracks WireGuard IP deallocation status
- **Error handling**: Handles network errors gracefully

## Technical Flow

### Normal Logout Flow:
1. User clicks "Logout" button
2. Confirmation dialog appears
3. User confirms logout
4. Qt app calls `POST /logout` with Bearer token
5. Backend validates token and user session
6. Backend finds user's WireGuard config in database
7. Backend removes peer from server WireGuard config file
8. Backend deletes WireGuard config record (frees IP)
9. Backend invalidates user session
10. Backend returns success response with WireGuard status
11. Qt app receives success response
12. Qt app clears local tokens and settings
13. Qt app restarts to show login screen

### Error Handling:
- **No network connection**: Proceeds with local logout only
- **API returns error**: Logs warning but continues with local logout
- **Invalid token**: Proceeds with local logout only
- **WireGuard cleanup fails**: Logout succeeds, but logs warning

## Database Impact

**Before logout**:
```sql
-- User has active WireGuard config
SELECT * FROM wireguard_configs WHERE user_id = 123;
-- Returns: id=456, user_id=123, allocated_ip='10.0.0.100', status='active'

-- IP is marked as used
SELECT * FROM ip_addresses WHERE ip_address = '10.0.0.100';
-- Returns: allocated to user
```

**After logout**:
```sql
-- WireGuard config is deleted
SELECT * FROM wireguard_configs WHERE user_id = 123;
-- Returns: (empty)

-- IP is freed and available for reallocation
SELECT * FROM ip_addresses WHERE ip_address = '10.0.0.100';
-- Returns: available for allocation
```

## Key Benefits

1. **Proper Resource Management**: WireGuard IPs are automatically freed on logout
2. **Prevents IP Exhaustion**: Available IP pool is properly maintained
3. **Clean User Sessions**: No stale VPN configurations remain after logout
4. **Improved User Experience**: Users can immediately rejoin VPN after logout/login
5. **Robust Error Handling**: System works even when backend is temporarily unavailable
6. **Complete Audit Trail**: All logout actions are properly logged

## Testing Verification

To verify the implementation works:

1. **Login as User A** → Note allocated WireGuard IP
2. **Check database** → Verify IP is allocated to User A
3. **Logout User A** → Application should call logout API
4. **Check backend logs** → Should show WireGuard config revocation
5. **Check database** → IP should be freed and available
6. **Login as User B** → Should be able to get same IP if no other users
7. **Login as User A again** → Should get new IP allocation (not cached)

## Files Modified

### Backend:
- `backend/app/routers/auth_routes.py` - Enhanced logout endpoint

### Qt Application:
- `include/UserProfileWidget.h` - Added logout API methods
- `src/UserProfileWidget.cpp` - Implemented API-first logout flow

## Backward Compatibility

- **Existing logout API consumers**: Still receive same basic response structure
- **Legacy client applications**: Will continue to work with enhanced logout endpoint
- **Database schema**: No schema changes required (uses existing tables)

## Security Considerations

- **Token validation**: Logout API validates Bearer token before cleanup
- **User isolation**: Users can only revoke their own WireGuard configs
- **Graceful degradation**: Local logout works even if backend is unavailable
- **Session invalidation**: Both local and server sessions are properly cleared

## Future Enhancements

1. **Batch IP cleanup**: Periodic cleanup of expired WireGuard configs
2. **IP allocation metrics**: Track IP usage patterns
3. **Advanced user management**: Admin APIs for managing user VPN access
4. **Audit logging**: Enhanced logging for compliance requirements

---

**Implementation Date**: September 5, 2025
**Status**: ✅ Complete and Tested
**Impact**: High - Resolves critical resource management issue
