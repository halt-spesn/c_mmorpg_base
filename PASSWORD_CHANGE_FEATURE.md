# Password Change Feature

## Overview
The password change feature allows users to change their account password through the in-game settings menu.

## How to Use

1. **Open Settings**: Click the "Settings" button at the bottom of the game screen
2. **Locate Change Password**: Scroll down in the settings menu to find the "Change Password" button (purple color)
3. **Click Change Password**: This opens a popup dialog with three fields
4. **Fill in the Form**:
   - **Current Password**: Enter your existing password
   - **New Password**: Enter your desired new password (must be 8+ characters)
   - **Confirm Password**: Re-enter your new password to confirm
5. **Show/Hide Passwords**: Each field has a checkbox to toggle password visibility
6. **Submit**: Click the "Change" button to submit your request
7. **Cancel**: Click the "Cancel" button to close without changes

## Validation Rules

### Client-Side Validation
- Current password field cannot be empty
- New password must be at least 8 characters long
- New password must match the confirm password field

### Server-Side Validation
- Current password must match the password in the database
- New password must be at least 8 characters long
- New password must match the confirm password

## Error Messages

You may see the following error messages:
- "Current password required." - You didn't enter your current password
- "New password must be 8+ chars." - Your new password is too short
- "Passwords don't match." - New password and confirm password don't match
- "Current password incorrect." - The current password you entered is wrong
- "User not found." - Database error (contact admin)
- "Database error." - Database update failed (contact admin)

## Success Message

When the password is changed successfully, you'll see:
- "Password changed successfully!"

The popup will automatically close and you can continue using your account with the new password.

## Technical Details

### Implementation
- **Packet Types**: `PACKET_CHANGE_PASSWORD_REQUEST`, `PACKET_CHANGE_PASSWORD_RESPONSE`
- **Database**: Password is updated in the `users` table
- **Security**: 
  - Current password is verified before allowing change
  - Uses prepared statements to prevent SQL injection
  - **Note**: This feature follows the existing authentication system's security model (plaintext passwords). For production use, the entire authentication system should be upgraded to use password hashing (bcrypt/Argon2) and secure transmission (TLS/SSL).

### Code Locations
- **Client**: `client_sdl.c` - Password change UI and request logic
- **Server**: `server.c` - `process_password_change()` function
- **Protocol**: `common.h` - Packet type definitions

### Field IDs
- Current Password: `active_field == FIELD_PASSWORD_CURRENT`
- New Password: `active_field == FIELD_PASSWORD_NEW`
- Confirm Password: `active_field == FIELD_PASSWORD_CONFIRM`
