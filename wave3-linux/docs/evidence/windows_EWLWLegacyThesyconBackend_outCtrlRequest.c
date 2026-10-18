// FUN_18009a0e0 @ 18009a0e0 (references "EWLW::EWLWLegacyThesyconBackend::outCtrlRequest")

/* WARNING: Function: __security_check_cookie replaced with injection: security_check_cookie */
/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

uint FUN_18009a0e0(longlong param_1,byte param_2,undefined1 param_3,undefined2 param_4,
                  ushort param_5,undefined8 param_6,ushort param_7)

{
  int iVar1;
  char **ppcVar2;
  undefined1 auStack_a8 [32];
  char **local_88;
  undefined2 local_80;
  ushort local_78;
  undefined8 local_70;
  uint *local_68;
  undefined4 local_60;
  char *local_58;
  undefined8 local_50;
  char *local_48;
  undefined8 local_40;
  undefined4 local_38;
  undefined4 uStack_34;
  undefined4 uStack_30;
  undefined4 uStack_2c;
  uint local_28 [2];
  ulonglong local_20;
  
  local_20 = DAT_180307a00 ^ (ulonglong)auStack_a8;
  local_28[0] = 0;
  if (*(int *)(param_1 + 8) == 0) {
    local_50 = 0x16;
    local_58 = "outCtrlRequest failed.";
    ppcVar2 = &local_48;
    local_40 = 0x2f;
    local_48 = "EWLW::EWLWLegacyThesyconBackend::outCtrlRequest";
    local_88 = &local_58;
  }
  else {
    if (*(code **)(DAT_18030fa20 + 0x178) != (code *)0x0) {
      local_60 = 500;
      local_68 = local_28;
      local_70 = param_6;
      local_78 = param_7;
      local_88 = (char **)CONCAT62(local_88._2_6_,
                                   (ushort)*(byte *)(param_1 + 0xc) | param_5 & 0xff00);
      local_80 = param_4;
      iVar1 = (**(code **)(DAT_18030fa20 + 0x178))(*(int *)(param_1 + 8),1,param_2 ^ 1,param_3);
      if ((iVar1 == 0) && (local_28[0] == param_7)) {
        return local_28[0];
      }
    }
    local_40 = 0x27;
    local_48 = "TUSBAUDIO_ClassVendorRequestOut failed.";
    ppcVar2 = &local_58;
    local_50 = 0x2f;
    local_58 = "EWLW::EWLWLegacyThesyconBackend::outCtrlRequest";
    local_88 = &local_48;
  }
  local_38 = PTR_s_EWLWLegacyThesyconBackend_180269e58._0_4_;
  uStack_34 = PTR_s_EWLWLegacyThesyconBackend_180269e58._4_4_;
  uStack_30 = _UNK_180269e60;
  uStack_2c = _UNK_180269e64;
  thunk_FUN_180084600(1,&local_38,ppcVar2);
  return 0xffffffff;
}


