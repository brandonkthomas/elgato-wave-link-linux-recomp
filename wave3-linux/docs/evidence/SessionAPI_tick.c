// LWT::SessionAPI::Impl::tick @ 00041040

/* LWT::SessionAPI::Impl::tick(LWT::LewittDevice const&) */

void __thiscall LWT::SessionAPI::Impl::tick(Impl *this,LewittDevice *param_1)

{
  long *plVar1;
  char *pcVar2;
  long lVar3;
  char *pcVar4;
  ushort uVar5;
  int iVar6;
  size_t sVar7;
  char *pcVar8;
  long lVar9;
  long lVar10;
  ulong uVar11;
  long lVar12;
  long lVar13;
  undefined **local_90;
  long lStack_88;
  long *local_80;
  undefined **local_78;
  long lStack_70;
  long *local_68;
  
  lVar9 = *(long *)(this + 8);
  lVar10 = *(long *)(this + 0x10);
  if (lVar9 == lVar10) {
    if (lVar9 != lVar10) {
LAB_000410f8:
      if (*(char *)(lVar9 + 0x20) == '\x01') {
        if ((*(char *)(lVar9 + 0x58) == '\x01') && (*(char *)(lVar9 + 0x78) == '\x01')) {
          plVar1 = (long *)(lVar9 + 0x40);
          local_78 = &PTR_messageId_008de7b8;
          lStack_70 = lVar9;
          local_68 = plVar1;
          LewittDevice::read(param_1,(IMessage *)&local_78);
          if (*(char *)(lVar9 + 0x98) == '\x01') {
            if ((*(byte *)(lVar9 + 0x78) & 1) == 0) goto LAB_00041304;
            iVar6 = _memcmp(*(void **)(lVar9 + 0x80),*(void **)(lVar9 + 0x60),
                            *(long *)(lVar9 + 0x88) - (long)*(void **)(lVar9 + 0x80));
            if (iVar6 != 0) {
              if (*(char *)(lVar9 + 0x58) == '\x01') {
                lVar3 = *(long *)(lVar9 + 0x30);
                for (lVar13 = *(long *)(lVar9 + 0x28); lVar13 != lVar3; lVar13 = lVar13 + 0xb0) {
                  uVar11 = (ulong)*(ushort *)(lVar13 + 0x10);
                  lVar12 = *(long *)(lVar9 + 0x80);
                  uVar5 = *(ushort *)(lVar13 + 0x12);
                  iVar6 = _memcmp((void *)(*(long *)(lVar9 + 0x60) + uVar11),
                                  (void *)(lVar12 + uVar11),(ulong)uVar5);
                  if (iVar6 != 0) {
                    _memcpy((void *)(*plVar1 + uVar11),(void *)(lVar12 + uVar11),(ulong)uVar5);
                  }
                }
              }
              if ((*(byte *)(lVar9 + 0x58) & 1) == 0) goto LAB_00041304;
              local_90 = &PTR_messageId_008de7b8;
              lStack_88 = lVar9;
              local_80 = plVar1;
              LewittDevice::write(param_1,(IMessage *)&local_90);
            }
          }
        }
        else if (*(char *)(lVar9 + 0x98) == '\x01') {
          pcVar2 = *(char **)(lVar9 + 0x80);
          pcVar4 = *(char **)(lVar9 + 0x88);
          if (pcVar2 != pcVar4) {
            pcVar8 = pcVar2;
            for (uVar11 = (long)pcVar4 - (long)pcVar2 & 3; uVar11 != 0; uVar11 = uVar11 - 1) {
              if (*pcVar8 != '\0') goto LAB_00041228;
              pcVar8 = pcVar8 + 1;
            }
            if (3 < (ulong)((long)pcVar4 - (long)pcVar2)) {
              do {
                if (((*pcVar8 != '\0') || (pcVar8[1] != '\0')) ||
                   ((pcVar8[2] != '\0' || (pcVar8[3] != '\0')))) goto LAB_00041228;
                pcVar8 = pcVar8 + 4;
              } while (pcVar8 != pcVar4);
            }
          }
        }
      }
      goto LAB_000410ec;
    }
  }
  else {
    do {
      if (((*(char *)(lVar9 + 0x20) == '\x01') && (*(char *)(lVar9 + 0x58) == '\x01')) &&
         (*(char *)(lVar9 + 0x78) == '\x01')) {
        sVar7 = *(long *)(lVar9 + 0x48) - (long)*(void **)(lVar9 + 0x40);
        if (sVar7 != 0) {
          _memmove(*(void **)(lVar9 + 0x60),*(void **)(lVar9 + 0x40),sVar7);
        }
      }
      lVar9 = lVar9 + 0xa0;
    } while (lVar9 != lVar10);
    lVar9 = *(long *)(this + 8);
    lVar10 = *(long *)(this + 0x10);
    if (lVar9 != lVar10) goto LAB_000410f8;
  }
  goto LAB_000412a0;
LAB_00041228:
  local_68 = (long *)(lVar9 + 0x80);
  local_78 = &PTR_messageId_008de7b8;
  lStack_70 = lVar9;
  LewittDevice::write(param_1,(IMessage *)&local_78);
  if ((*(byte *)(lVar9 + 0x98) & 1) == 0) {
LAB_00041304:
                    /* WARNING: Subroutine does not return */
    std::__throw_bad_optional_access_abi_nqe210106_();
  }
  sVar7 = *(long *)(lVar9 + 0x88) - (long)*(void **)(lVar9 + 0x80);
  if (0 < (long)sVar7) {
    _bzero(*(void **)(lVar9 + 0x80),sVar7);
  }
LAB_000410ec:
  lVar9 = lVar9 + 0xa0;
  if (lVar9 == lVar10) goto LAB_00041290;
  goto LAB_000410f8;
LAB_00041290:
  lVar9 = *(long *)(this + 8);
  lVar10 = *(long *)(this + 0x10);
LAB_000412a0:
  for (; lVar9 != lVar10; lVar9 = lVar9 + 0xa0) {
    if (((*(char *)(lVar9 + 0x20) == '\x01') && (*(char *)(lVar9 + 0x58) == '\x01')) &&
       (*(char *)(lVar9 + 0x98) == '\x01')) {
      sVar7 = *(long *)(lVar9 + 0x48) - (long)*(void **)(lVar9 + 0x40);
      if (sVar7 != 0) {
        _memmove(*(void **)(lVar9 + 0x80),*(void **)(lVar9 + 0x40),sVar7);
      }
    }
  }
  return;
}


