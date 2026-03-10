#pragma once

#include "..\Main.h"
#include "..\OnScreenKeyboard.h"
#include "..\StoreList.h"
#include "Scene.h"

// ---------------------------------------------------------------------------
// StoreManagerScene - full-screen store URL manager
//
// Shows list of configured stores.  Active store has green accent + ACTIVE
// badge. Selected store has pink accent.
//
// Controls
//   A              Set selected store as active
//   Y              Edit selected store (name then URL via OSK)
//   X              Delete selected store (blocked if only 1)
//   Black button   Add new store
//   B / Start      Go back (saves automatically)
//   D-Pad Up/Down  Navigate
// ---------------------------------------------------------------------------
class StoreManagerScene : public Scene {
public:
  StoreManagerScene();
  virtual ~StoreManagerScene();
  virtual void Render();
  virtual void Update();
  virtual void OnResume();

private:
  void RenderHeader();
  void RenderFooter();
  void RenderList();
  void RenderStoreRow(float x, float y, float w, bool selected, int32_t idx);

  void SetActive(int32_t idx);
  void BeginDelete(int32_t idx);
  void BeginAdd();
  void BeginEdit(int32_t idx);

  // Two-step OSK flow: name first, then URL
  enum OskStep { OskNone,
    OskName,
    OskUrl };
  void OskOpen(OskStep step, const std::string &prefill);
  void OskConfirmed(const std::string &value);

  int32_t mSelectedRow;
  int32_t mScrollOffset; // index of first visible row

  // OSK
  OnScreenKeyboard mKeyboard;
  OskStep mOskStep;
  bool mOskIsEdit; // true = editing, false = adding
  int32_t mOskEditIndex;
  std::string mOskPendingName;

  // Delete confirm dialog
  bool mDeleteOpen;
  int32_t mDeleteIndex;

  static const float ROW_H;
};
