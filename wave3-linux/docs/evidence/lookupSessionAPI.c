// LWT::BaseWaveDevice::Impl::lookupSessionAPI @ 003e3330

/* LWT::BaseWaveDevice::Impl::lookupSessionAPI(LWT::BaseWaveDevice const&,
   std::span<std::pair<LWT::LewittDeviceVersionPair, std::span<LWT::PackedStructView const,
   18446744073709551615ul> > const, 18446744073709551615ul>) */

void LWT::BaseWaveDevice::Impl::lookupSessionAPI
               (undefined8 *param_1_00,long param_1,ushort *param_3,long param_4)

{
  undefined4 uVar1;
  undefined4 *puVar2;
  int iVar3;
  ulong uVar4;
  void *pvVar5;
  Impl *pIVar6;
  long lVar7;
  ulong uVar8;
  bool bVar9;
  long lVar10;
  long lVar11;
  ushort *puVar12;
  undefined4 *puVar13;
  undefined4 *puVar14;
  ushort *puVar15;
  size_t sVar16;
  undefined4 *puVar17;
  undefined4 *puVar18;
  void *local_f0;
  char local_d9;
  void *local_d8;
  char local_c1;
  void *local_c0;
  char local_a9;
  uint local_9c;
  char local_90;
  undefined4 local_88;
  undefined4 uStack_84;
  undefined4 uStack_80;
  undefined4 uStack_7c;
  undefined4 uStack_78;
  undefined4 uStack_74;
  long local_70;
  
  local_70 = *(long *)PTR____stack_chk_guard_008dc420;
  uVar4 = (**(code **)(**(long **)(param_1 + 0xb0) + 0x68))();
  if ((uVar4 & 1) == 0) {
    LewittDevice::deviceInfo();
    if (local_90 == '\x01') {
      lVar10 = param_4 * 0x18;
      puVar12 = param_3;
      if (param_4 == 0) {
        puVar13 = (undefined4 *)0x0;
LAB_003e356c:
        if (puVar12 == param_3 + param_4 * 0xc) goto LAB_003e36d8;
        if (((*PTR_s_instance_008dc6e8 & 1) == 0) &&
           (iVar3 = ___cxa_guard_acquire(PTR_s_instance_008dc6e8), iVar3 != 0)) {
          spdlog::details::registry::registry((registry *)PTR_s_instance_008dc7b0);
          ___cxa_atexit(spdlog::details::registry::~registry,PTR_s_instance_008dc7b0,0);
          ___cxa_guard_release(PTR_s_instance_008dc6e8);
        }
        local_88 = 0x8d3c2f;
        uStack_84 = 0;
        uStack_80 = 0x4c;
        uStack_78 = 0x8d3c98;
        uStack_74 = 0;
        spdlog::logger::log(*(undefined8 *)(PTR_s_instance_008dc7b0 + 0x158),&local_88,0,
                            "Fallback to APIv1.0",0x13);
        pIVar6 = operator_new(0x28);
        SessionAPI::Impl::Impl
                  (pIVar6,*(undefined4 *)puVar12,*(undefined8 *)(puVar12 + 4),
                   *(undefined8 *)(puVar12 + 8));
        bVar9 = false;
        *param_1_00 = pIVar6;
joined_r0x003e35f0:
        if (puVar13 != (undefined4 *)0x0) {
          operator_delete(puVar13);
        }
        if (bVar9) goto LAB_003e3600;
LAB_003e3614:
        bVar9 = false;
      }
      else {
        puVar18 = (undefined4 *)0x0;
        puVar17 = (undefined4 *)0x0;
        lVar11 = lVar10;
        puVar14 = (undefined4 *)0x0;
        puVar15 = param_3;
        do {
          puVar13 = puVar14;
          if (((uint)*puVar15 == (local_9c & 0xffff)) && ((uint)puVar15[1] <= local_9c >> 0x10)) {
            uVar1 = *(undefined4 *)puVar15;
            uStack_7c = (undefined4)*(undefined8 *)(puVar15 + 8);
            uStack_78 = (undefined4)((ulong)*(undefined8 *)(puVar15 + 8) >> 0x20);
            uStack_84 = (undefined4)*(undefined8 *)(puVar15 + 4);
            uStack_80 = (undefined4)((ulong)*(undefined8 *)(puVar15 + 4) >> 0x20);
            if (puVar18 < puVar17) {
              *puVar18 = uVar1;
              puVar18[5] = uStack_78;
              *(ulong *)(puVar18 + 3) = CONCAT44(uStack_7c,uStack_80);
              *(ulong *)(puVar18 + 1) = CONCAT44(uStack_84,local_88);
              puVar18 = puVar18 + 6;
            }
            else {
              sVar16 = (long)puVar18 - (long)puVar14;
              uVar4 = ((long)sVar16 >> 3) * -0x5555555555555555 + 1;
              if (0xaaaaaaaaaaaaaaa < uVar4) {
                    /* WARNING: Subroutine does not return */
                std::vector<LWT::LewittDeviceID,std::allocator<LWT::LewittDeviceID>>::
                __throw_length_error_abi_nqe210106_();
              }
              lVar7 = (long)puVar17 - (long)puVar14 >> 3;
              uVar8 = lVar7 * 0x5555555555555556;
              if (uVar8 < uVar4 || uVar8 - uVar4 == 0) {
                uVar8 = uVar4;
              }
              if (0x555555555555554 < (ulong)(lVar7 * -0x5555555555555555)) {
                uVar8 = 0xaaaaaaaaaaaaaaa;
              }
              if (0xaaaaaaaaaaaaaaa < uVar8) {
                    /* WARNING: Subroutine does not return */
                std::__throw_bad_array_new_length_abi_nqe210106_();
              }
              pvVar5 = operator_new(uVar8 * 0x18);
              puVar13 = (undefined4 *)((long)pvVar5 + sVar16);
              puVar17 = (undefined4 *)((long)pvVar5 + uVar8 * 0x18);
              *puVar13 = uVar1;
              *(ulong *)(puVar13 + 3) = CONCAT44(uStack_7c,uStack_80);
              *(ulong *)(puVar13 + 1) = CONCAT44(uStack_84,local_88);
              puVar13[5] = uStack_78;
              puVar18 = puVar13 + 6;
              uVar4 = SUB168(SEXT816((long)sVar16) * SEXT816(-0x2aaaaaaaaaaaaaab),8);
              puVar13 = puVar13 + ((uVar4 >> 2) - ((long)uVar4 >> 0x3f)) * 6;
              _memcpy(puVar13,puVar14,sVar16);
              if (puVar14 != (undefined4 *)0x0) {
                operator_delete(puVar14);
              }
            }
          }
          puVar15 = puVar15 + 0xc;
          lVar11 = lVar11 + -0x18;
          puVar14 = puVar13;
        } while (lVar11 != 0);
        puVar17 = puVar13;
        if (puVar18 == puVar13) {
          do {
            if (*puVar12 == 1 && puVar12[1] == 0) goto LAB_003e356c;
            lVar10 = lVar10 + -0x18;
            puVar12 = puVar12 + 0xc;
          } while (lVar10 != 0);
LAB_003e36d8:
          if (((*PTR_s_instance_008dc6e8 & 1) == 0) &&
             (iVar3 = ___cxa_guard_acquire(PTR_s_instance_008dc6e8), iVar3 != 0)) {
            spdlog::details::registry::registry((registry *)PTR_s_instance_008dc7b0);
            ___cxa_atexit(spdlog::details::registry::~registry,PTR_s_instance_008dc7b0,0);
            ___cxa_guard_release(PTR_s_instance_008dc6e8);
          }
          local_88 = 0;
          uStack_84 = 0;
          uStack_80 = 0;
          uStack_7c = 0;
          uStack_78 = 0;
          uStack_74 = 0;
          spdlog::logger::log(*(undefined8 *)(PTR_s_instance_008dc7b0 + 0x158),&local_88,4,
                              "Compatible API not found!",0x19);
          bVar9 = true;
          goto joined_r0x003e35f0;
        }
        while (puVar2 = puVar14, puVar14 = puVar17 + 6, puVar14 != puVar18) {
          puVar12 = (ushort *)((long)puVar17 + 0x1a);
          puVar17 = puVar14;
          if (*puVar12 <= *(ushort *)((long)puVar2 + 2)) {
            puVar14 = puVar2;
          }
        }
        pIVar6 = operator_new(0x28);
        SessionAPI::Impl::Impl
                  (pIVar6,*puVar2,*(undefined8 *)(puVar2 + 2),*(undefined8 *)(puVar2 + 4));
        *param_1_00 = pIVar6;
        if (puVar13 == (undefined4 *)0x0) goto LAB_003e3614;
        operator_delete(puVar13);
        bVar9 = false;
      }
LAB_003e3624:
      if (local_a9 < '\0') {
        operator_delete(local_c0);
      }
      if (local_c1 < '\0') {
        operator_delete(local_d8);
      }
      if (local_d9 < '\0') {
        operator_delete(local_f0);
      }
    }
    else {
LAB_003e3600:
      bVar9 = true;
      if (local_90 == '\x01') goto LAB_003e3624;
    }
    if (!bVar9) goto LAB_003e3644;
  }
  *param_1_00 = 0;
LAB_003e3644:
  if (*(long *)PTR____stack_chk_guard_008dc420 == local_70) {
    return;
  }
                    /* WARNING: Subroutine does not return */
  ___stack_chk_fail();
}


