// RUN: heir-opt --ckks-to-openfhe %s | FileCheck %s

#encoding = #lwe.polynomial_evaluation_encoding<cleartext_start=30, cleartext_bitwidth=3>

#my_poly = #polynomial.int_polynomial<1 + x**1024>
// cmod is 64153 * 2521
#ring1 = #polynomial.ring<coefficientType=!mod_arith.int<161729713:i32>, polynomialModulus=#my_poly>
#ring2 = #polynomial.ring<coefficientType=!mod_arith.int<2521:i32>, polynomialModulus=#my_poly>

#params1 = #lwe.rlwe_params<dimension=2, ring=#ring1>
#params2 = #lwe.rlwe_params<dimension=4, ring=#ring1>
#params3 = #lwe.rlwe_params<dimension=2, ring=#ring2>
#params4 = #lwe.rlwe_params<dimension=3, ring=#ring1>

!ct = !lwe.rlwe_ciphertext<encoding=#encoding, rlwe_params=#params1, underlying_type=i3>
!ct_dim = !lwe.rlwe_ciphertext<encoding=#encoding, rlwe_params=#params2, underlying_type=i3>
!ct_level = !lwe.rlwe_ciphertext<encoding=#encoding, rlwe_params=#params3, underlying_type=i3>
!ct_level3 = !lwe.rlwe_ciphertext<encoding=#encoding, rlwe_params=#params4, underlying_type=i3>

// CHECK: module
module {
  // CHECK-LABEL: @test_fn
  // CHECK-SAME: ([[X:%.+]]: [[T:.*161729713.*]]) -> [[T]]
  func.func @test_fn(%x : !ct) -> !ct {
    // CHECK: return [[X]] : [[T]]
    return %x : !ct
  }

  // CHECK-LABEL: @test_ops
  // CHECK-SAME: ([[C:%.+]]: [[S:.*crypto_context]], [[X:%.+]]: [[T:.*161729713.*]], [[Y:%.+]]: [[T]])
  func.func @test_ops(%x : !ct, %y : !ct) {
    // CHECK: %[[v1:.*]] = openfhe.negate [[C]], %[[x1:.*]] : ([[S]], [[T]]) -> [[T]]
    %negate = ckks.negate %x  : !ct
    // CHECK: %[[v2:.*]] = openfhe.add [[C]], %[[x2:.*]], %[[y2:.*]]: ([[S]], [[T]], [[T]]) -> [[T]]
    %add = ckks.add %x, %y  : !ct
    // CHECK: %[[v3:.*]] = openfhe.sub [[C]], %[[x3:.*]], %[[y3:.*]]: ([[S]], [[T]], [[T]]) -> [[T]]
    %sub = ckks.sub %x, %y  : !ct
    // CHECK: %[[v4:.*]] = openfhe.mul_no_relin [[C]], %[[x4:.*]], %[[y4:.*]]: ([[S]], [[T]], [[T]]) -> [[T2:.*]]
    %mul = ckks.mul %x, %y  : (!ct, !ct) -> !ct_level3
    // CHECK: %[[v5:.*]] = openfhe.rot [[C]], %[[x5:.*]] {index = 4 : i64}
    // CHECK-SAME: ([[S]], [[T]]) -> [[T]]
    %rot = ckks.rotate %x { offset = 4 } : !ct
    return
  }

  // CHECK-LABEL: @test_relin
  // CHECK-SAME: ([[C:.*]]: [[S:.*crypto_context]], [[X:%.+]]: [[T:.*dimension = 4.*]])
  func.func @test_relin(%x : !ct_dim) {
    // CHECK: %[[v6:.*]] = openfhe.relin [[C]], %[[x6:.*]]: ([[S]], [[T]]) -> [[T2:.*]]
    %relin = ckks.relinearize %x  {
      from_basis = array<i32: 0, 1, 2, 3>, to_basis = array<i32: 0, 1>
    }: !ct_dim -> !ct
    return
  }
}
