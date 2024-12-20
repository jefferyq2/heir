#include "lib/Dialect/LWE/IR/LWEDialect.h"

#include <cstdint>
#include <optional>

#include "lib/Dialect/LWE/IR/LWEAttributes.h"
#include "lib/Dialect/LWE/IR/LWEOps.h"
#include "lib/Dialect/LWE/IR/LWETypes.h"
#include "lib/Dialect/ModArith/IR/ModArithTypes.h"
#include "lib/Dialect/Polynomial/IR/PolynomialAttributes.h"
#include "lib/Dialect/Polynomial/IR/PolynomialTypes.h"
#include "llvm/include/llvm/ADT/STLFunctionalExtras.h"   // from @llvm-project
#include "llvm/include/llvm/ADT/TypeSwitch.h"            // from @llvm-project
#include "llvm/include/llvm/Support/Casting.h"           // from @llvm-project
#include "llvm/include/llvm/Support/ErrorHandling.h"     // from @llvm-project
#include "mlir/include/mlir/IR/Diagnostics.h"            // from @llvm-project
#include "mlir/include/mlir/IR/DialectImplementation.h"  // from @llvm-project

// Generated definitions
#include "lib/Dialect/LWE/IR/LWEDialect.cpp.inc"
#include "lib/Dialect/LWE/IR/LWEEnums.cpp.inc"
#include "mlir/include/mlir/IR/Location.h"            // from @llvm-project
#include "mlir/include/mlir/IR/Types.h"               // from @llvm-project
#include "mlir/include/mlir/Support/LLVM.h"           // from @llvm-project
#include "mlir/include/mlir/Support/LogicalResult.h"  // from @llvm-project
#define GET_ATTRDEF_CLASSES
#include "lib/Dialect/LWE/IR/LWEAttributes.cpp.inc"
#define GET_TYPEDEF_CLASSES
#include "lib/Dialect/LWE/IR/LWETypes.cpp.inc"
#define GET_OP_CLASSES
#include "lib/Dialect/LWE/IR/LWEOps.cpp.inc"

namespace mlir {
namespace heir {
namespace lwe {

void LWEDialect::initialize() {
  addAttributes<
#define GET_ATTRDEF_LIST
#include "lib/Dialect/LWE/IR/LWEAttributes.cpp.inc"
      >();
  addTypes<
#define GET_TYPEDEF_LIST
#include "lib/Dialect/LWE/IR/LWETypes.cpp.inc"
      >();
  addOperations<
#define GET_OP_LIST
#include "lib/Dialect/LWE/IR/LWEOps.cpp.inc"
      >();
}

LogicalResult RMulOp::verify() {
  auto x = getLhs().getType();
  auto y = getRhs().getType();
  if (x.getRlweParams().getDimension() != y.getRlweParams().getDimension()) {
    return emitOpError() << "input dimensions do not match";
  }
  auto out = getOutput().getType();
  if (out.getRlweParams().getDimension() !=
      y.getRlweParams().getDimension() + x.getRlweParams().getDimension() - 1) {
    return emitOpError() << "output.dim == x.dim + y.dim - 1 does not hold";
  }
  return success();
}

LogicalResult RMulOp::inferReturnTypes(
    MLIRContext* ctx, std::optional<Location>, RMulOp::Adaptor adaptor,
    SmallVectorImpl<Type>& inferredReturnTypes) {
  auto x = cast<lwe::RLWECiphertextType>(adaptor.getLhs().getType());
  auto y = cast<lwe::RLWECiphertextType>(adaptor.getRhs().getType());
  auto newDim =
      x.getRlweParams().getDimension() + y.getRlweParams().getDimension() - 1;
  inferredReturnTypes.push_back(lwe::RLWECiphertextType::get(
      ctx, x.getEncoding(),
      lwe::RLWEParamsAttr::get(ctx, newDim, x.getRlweParams().getRing()),
      x.getUnderlyingType()));
  return success();
}

LogicalResult BitFieldEncodingAttr::verifyEncoding(
    ArrayRef<int64_t> shape, Type elementType,
    ::llvm::function_ref<::mlir::InFlightDiagnostic()> emitError) const {
  if (!elementType.isSignlessInteger()) {
    return emitError() << "Tensors with a bit_field_encoding must have "
                       << "signless integer element type, but found "
                       << elementType;
  }

  unsigned plaintextBitwidth = elementType.getIntOrFloatBitWidth();
  unsigned cleartextBitwidth = getCleartextBitwidth();
  if (plaintextBitwidth < cleartextBitwidth)
    return emitError() << "The tensor element type's bitwidth "
                       << plaintextBitwidth
                       << " is too small to store the cleartext, "
                       << "which has bit width " << cleartextBitwidth << "";

  auto cleartextStart = getCleartextStart();
  if (cleartextStart < 0 || cleartextStart >= plaintextBitwidth)
    return emitError() << "Attribute's cleartext starting bit index ("
                       << cleartextStart << ") is outside the legal range [0, "
                       << plaintextBitwidth - 1 << "]";

  // It may be worth adding some sort of warning notification if the attribute
  // allocates no bits for noise, since this would be effectively useless for
  // FHE.
  return success();
}

LogicalResult UnspecifiedBitFieldEncodingAttr::verifyEncoding(
    ArrayRef<int64_t> shape, Type elementType,
    ::llvm::function_ref<::mlir::InFlightDiagnostic()> emitError) const {
  if (!elementType.isSignlessInteger()) {
    return emitError() << "Tensors with a bit_field_encoding must have "
                       << "signless integer element type, but found "
                       << elementType;
  }

  unsigned plaintextBitwidth = elementType.getIntOrFloatBitWidth();
  unsigned cleartextBitwidth = getCleartextBitwidth();
  if (plaintextBitwidth < cleartextBitwidth)
    return emitError() << "The tensor element type's bitwidth "
                       << plaintextBitwidth
                       << " is too small to store the cleartext, "
                       << "which has bit width " << cleartextBitwidth << "";

  return success();
}

LogicalResult requirePolynomialElementTypeFits(
    Type elementType, llvm::StringRef encodingName, unsigned cleartextBitwidth,
    unsigned cleartextStart,
    llvm::function_ref<::mlir::InFlightDiagnostic()> emitError) {
  if (!mlir::isa<::mlir::heir::polynomial::PolynomialType>(elementType)) {
    return emitError()
           << "Tensors with encoding " << encodingName
           << " must have `polynomial.polynomial` element type, but found "
           << elementType << "\n";
  }
  ::mlir::heir::polynomial::PolynomialType polyType =
      llvm::cast<::mlir::heir::polynomial::PolynomialType>(elementType);
  // The coefficient modulus takes the place of the plaintext bitwidth for
  // RLWE.
  auto coeffType = dyn_cast<mod_arith::ModArithType>(
      polyType.getRing().getCoefficientType());
  if (!coeffType) {
    return emitError()
           << "The polys in this tensor have a mod_arith coefficient type"
           << " but found " << polyType.getRing().getCoefficientType();
  }
  unsigned plaintextBitwidth =
      coeffType.getModulus().getType().getIntOrFloatBitWidth();

  if (plaintextBitwidth < cleartextBitwidth)
    return emitError() << "The polys in this tensor have a coefficient "
                       << "modulus with bitwidth " << plaintextBitwidth
                       << ", which too small to store the cleartext, "
                       << "which has bit width " << cleartextBitwidth << "";

  if (cleartextStart < 0 || cleartextStart >= plaintextBitwidth)
    return emitError() << "Attribute's cleartext starting bit index ("
                       << cleartextStart << ") is outside the legal range [0, "
                       << plaintextBitwidth - 1 << "]";

  return success();
}

LogicalResult PolynomialCoefficientEncodingAttr::verifyEncoding(
    ArrayRef<int64_t> shape, Type elementType,
    ::llvm::function_ref<::mlir::InFlightDiagnostic()> emitError) const {
  return requirePolynomialElementTypeFits(
      elementType, "poly_coefficient_encoding", getCleartextBitwidth(),
      getCleartextStart(), emitError);
}

LogicalResult PolynomialEvaluationEncodingAttr::verifyEncoding(
    ArrayRef<int64_t> shape, Type elementType,
    ::llvm::function_ref<::mlir::InFlightDiagnostic()> emitError) const {
  return requirePolynomialElementTypeFits(
      elementType, "poly_evaluation_encoding", getCleartextBitwidth(),
      getCleartextStart(), emitError);
}

LogicalResult InverseCanonicalEmbeddingEncodingAttr::verifyEncoding(
    ArrayRef<int64_t> shape, Type elementType,
    ::llvm::function_ref<::mlir::InFlightDiagnostic()> emitError) const {
  return requirePolynomialElementTypeFits(
      elementType, "inverse_canonical_embedding_encoding",
      getCleartextBitwidth(), getCleartextStart(), emitError);
}

LogicalResult TrivialEncryptOp::verify() {
  auto paramsAttr = this->getParamsAttr();
  auto outParamsAttr = this->getOutput().getType().getLweParams();

  if (paramsAttr != outParamsAttr) {
    return this->emitOpError()
           << "lwe_params attr must match on the op and "
              "the output type, but found op attr "
           << paramsAttr << " and output type attr " << outParamsAttr;
  }

  return success();
}

LogicalResult ReinterpretUnderlyingTypeOp::verify() {
  auto inputType = getInput().getType();
  auto outputType = getOutput().getType();
  if (inputType.getEncoding() != outputType.getEncoding() ||
      inputType.getRlweParams() != outputType.getRlweParams()) {
    return emitOpError()
           << "the only allowed difference in the input and output are in the "
              "underlying_type field, but found input type "
           << inputType << " and output type " << outputType;
  }

  return success();
}

// Verification for RLWE_EncryptOp
LogicalResult RLWEEncryptOp::verify() {
  Type keyType = getKey().getType();
  lwe::RLWEParamsAttr keyParams =
      llvm::TypeSwitch<Type, lwe::RLWEParamsAttr>(keyType)
          .Case<lwe::RLWEPublicKeyType, lwe::RLWESecretKeyType>(
              [](auto key) { return key.getRlweParams(); })
          .Default([](Type) {
            llvm_unreachable("impossible by type constraints");
            return nullptr;
          });

  lwe::RLWEParamsAttr outputParams = getOutput().getType().getRlweParams();
  if (outputParams != keyParams) {
    return emitOpError()
           << "RLWEEncryptOp input dimensions do not match. Keyparams: "
           << keyParams << ". Output ciphertext params: " << outputParams
           << ".";
  }
  return success();
}

LogicalResult ApplicationDataAttr::verify(
    ::llvm::function_ref<::mlir::InFlightDiagnostic()> emitError,
    mlir::Type messageType, Attribute overflow) {
  if (!mlir::isa<PreserveOverflowAttr, NoOverflowAttr>(overflow)) {
    return emitError() << "overflow must be either preserve_overflow or "
                       << "no_overflow, but found " << overflow << "\n";
  }

  return success();
}

LogicalResult PlaintextSpaceAttr::verify(
    ::llvm::function_ref<::mlir::InFlightDiagnostic()> emitError,
    mlir::heir::polynomial::RingAttr ring, Attribute encoding) {
  if (mlir::isa<FullCRTPackingEncodingAttr>(encoding)) {
    // For full CRT packing, the ring must be of the form x^n + 1 and the
    // modulus must be 1 mod n.
    auto polyMod = ring.getPolynomialModulus();
    auto poly = polyMod.getPolynomial();
    auto polyTerms = poly.getTerms();
    if (polyTerms.size() != 2) {
      return emitError() << "polynomial modulus must be of the form x^n + 1, "
                         << "but found " << polyMod << "\n";
    }
    const auto& constantTerm = polyTerms[0];
    const auto& constantCoeff = constantTerm.getCoefficient();
    if (!(constantTerm.getExponent().isZero() && constantCoeff.isOne() &&
          polyTerms[1].getCoefficient().isOne())) {
      return emitError() << "polynomial modulus must be of the form x^n + 1, "
                         << "but found " << polyMod << "\n";
    }
    // Check that the modulus is 1 mod n.
    auto modCoeffTy =
        llvm::dyn_cast<mod_arith::ModArithType>(ring.getCoefficientType());
    if (modCoeffTy) {
      APInt modulus = modCoeffTy.getModulus().getValue();
      unsigned n = poly.getDegree();
      if (!modulus.urem(APInt(modulus.getBitWidth(), n)).isOne()) {
        return emitError()
               << "modulus must be 1 mod n for full CRT packing, mod = "
               << modulus.getZExtValue() << " n = " << n << "\n";
      }
    }
  }

  return success();
}

LogicalResult NewLWECiphertextType::verify(
    llvm::function_ref<mlir::InFlightDiagnostic()> emitError,
    mlir::heir::lwe::ApplicationDataAttr, mlir::heir::lwe::PlaintextSpaceAttr,
    mlir::heir::lwe::CiphertextSpaceAttr ciphertextSpace,
    mlir::heir::lwe::KeyAttr keyAttr, mlir::heir::lwe::ModulusChainAttr) {
  if (keyAttr.getSlotIndex() != 0 && (ciphertextSpace.getSize() != 2)) {
    return emitError() << "a ciphertext with nontrivial slot rotation must "
                          "have size 2, but found size "
                       << ciphertextSpace.getSize();
  }
  return success();
}

}  // namespace lwe
}  // namespace heir
}  // namespace mlir
