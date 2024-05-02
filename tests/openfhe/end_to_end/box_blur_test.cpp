#include <cstdint>
#include <vector>

#include "gmock/gmock.h"                               // from @googletest
#include "gtest/gtest.h"                               // from @googletest
#include "src/core/include/lattice/hal/lat-backend.h"  // from @openfhe
#include "src/pke/include/constants.h"                 // from @openfhe
#include "src/pke/include/cryptocontext-fwd.h"         // from @openfhe
#include "src/pke/include/gen-cryptocontext.h"         // from @openfhe
#include "src/pke/include/key/keypair.h"               // from @openfhe
#include "src/pke/include/openfhe.h"                   // from @openfhe
#include "src/pke/include/scheme/bgvrns/gen-cryptocontext-bgvrns-params.h"  // from @openfhe
#include "src/pke/include/scheme/bgvrns/gen-cryptocontext-bgvrns.h"  // from @openfhe

// Generated headers (block clang-format from messing up order)
#include "tests/openfhe/end_to_end/box_blur_64x64_lib.h"

using ::testing::ContainerEq;

namespace mlir {
namespace heir {
namespace openfhe {

TEST(BinopsTest, TestInput1) {
  CCParams<CryptoContextBGVRNS> parameters;
  parameters.SetMultiplicativeDepth(2);
  parameters.SetPlaintextModulus(65537);
  CryptoContext<DCRTPoly> cryptoContext = GenCryptoContext(parameters);
  cryptoContext->Enable(PKE);
  cryptoContext->Enable(KEYSWITCH);
  cryptoContext->Enable(LEVELEDSHE);

  KeyPair<DCRTPoly> keyPair;
  keyPair = cryptoContext->KeyGen();
  cryptoContext->EvalRotateKeyGen(keyPair.secretKey, {65, 3968, 127, 4032, 63});

  int32_t n = cryptoContext->GetCryptoParameters()
                  ->GetElementParams()
                  ->GetCyclotomicOrder() /
              2;
  std::vector<int16_t> input;
  std::vector<int16_t> expected;
  input.reserve(n);
  expected.reserve(4096);

  // TODO(#645): support cyclic repetition in add-client-interface
  for (int i = 0; i < n; ++i) {
    // TODO(#669): investigate failure due to int16_t overflow
    //
    // figure out why tests fail when this changes to
    //
    //   input.push_back(i % 4096);
    //
    // all unequal values are negative and exactly off-by-one, suggesting
    // a problem with overflow. Perhaps my use of a cast to int16_t
    // during decoding?
    input.push_back(i % 32);
  }

  for (int row = 0; row < 64; ++row) {
    for (int col = 0; col < 64; ++col) {
      int16_t sum = 0;
      for (int di = -1; di < 2; ++di) {
        for (int dj = -1; dj < 2; ++dj) {
          int index = (row * 64 + col + di * 64 + dj) % 4096;
          if (index < 0) index += 4096;
          sum += input[index];
        }
      }
      expected.push_back(sum);
    }
  }

  auto inputEncrypted =
      box_blur__encrypt__arg0(cryptoContext, input, keyPair.publicKey);
  auto outputEncrypted = box_blur(cryptoContext, inputEncrypted);
  auto actual = box_blur__decrypt__result0(cryptoContext, outputEncrypted,
                                           keyPair.secretKey);

  EXPECT_THAT(actual, ContainerEq(expected));
}

}  // namespace openfhe
}  // namespace heir
}  // namespace mlir