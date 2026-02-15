//=============================================================================
// TextureHelper.h - Load and store media textures (Screenshot.jpg, Cover.jpg)
//=============================================================================

#pragma once

#include <xtl.h>
#include <xgraphics.h>

class TextureHelper
{
public:
    // Load Screenshot.jpg and Cover.jpg from Media folder. Returns S_OK on success.
    static HRESULT Init( LPDIRECT3DDEVICE8 pd3dDevice );

    // Return a copy of the texture (caller must Release). Returns NULL if not initialized or copy fails.
    static LPDIRECT3DTEXTURE8 GetScreenshot( LPDIRECT3DDEVICE8 pd3dDevice );
    static LPDIRECT3DTEXTURE8 GetCover( LPDIRECT3DDEVICE8 pd3dDevice );

private:
    static LPDIRECT3DTEXTURE8 s_pScreenshot;
    static LPDIRECT3DTEXTURE8 s_pCover;
    static LPDIRECT3DTEXTURE8 CopyTexture( LPDIRECT3DDEVICE8 pd3dDevice, LPDIRECT3DTEXTURE8 pSource );
};
