// Copyright © 2017-2021 Trust Wallet.
//
// This file is part of Trust. The full Trust copyright notice, including
// terms governing use, modification, and redistribution, is contained in the
// file LICENSE at the root of the source code distribution tree.

#include "HDWallet.h"
#include "Mnemonic.h"
#include "Bitcoin/Address.h"
#include "Bitcoin/CashAddress.h"
#include "Bitcoin/SegwitAddress.h"
#include "Ethereum/Address.h"
#include "HexCoding.h"
#include "PublicKey.h"
#include "Hash.h"
#include "Base58.h"
#include "Coin.h"

#include <gtest/gtest.h>

namespace TW {

const auto mnemonic1 = "ripple scissors kick mammal hire column oak again sun offer wealth tomorrow wagon turn fatal";
const auto password = "password";

#define EXPECT_EXCEPTION(statement, exceptionMsg) \
    try { \
        statement; \
        FAIL() << "No exception"; \
    } catch (const std::exception& ex) { \
        EXPECT_EQ(std::string(ex.what()), exceptionMsg); \
    } catch (...) { \
        FAIL() << "Not the expected exception"; \
    }

TEST(HDWallet, generate) {
    {
        HDWallet wallet = HDWallet(128, password);
        EXPECT_TRUE(Mnemonic::isValid(wallet.mnemonic));
        EXPECT_EQ(wallet.passphrase, password);
        EXPECT_EQ(wallet.entropy.size(), 16);
    }
    {
        HDWallet wallet = HDWallet(256, password);
        EXPECT_TRUE(Mnemonic::isValid(wallet.mnemonic));
        EXPECT_EQ(wallet.passphrase, password);
        EXPECT_EQ(wallet.entropy.size(), 33);
    }
}

TEST(HDWallet, generateInvalid) {
    EXPECT_EXCEPTION(HDWallet(64, password), "Invalid strength");
    EXPECT_EXCEPTION(HDWallet(129, password), "Invalid strength");
    EXPECT_EXCEPTION(HDWallet(512, password), "Invalid strength");
}

TEST(HDWallet, createFromMnemonic) {
    {
        HDWallet wallet = HDWallet(mnemonic1, password);
        EXPECT_EQ(hex(wallet.seed), "97fe6f346b022aa298a2b3e26dfafe75eba39f66610e04ec13528fb118245332f6e1e5801b03d387c2d9e874f4ae86738d6a048c65ba36f014988edbf7b63540");
        EXPECT_EQ(wallet.mnemonic, mnemonic1);
        EXPECT_EQ(wallet.passphrase, password);
        EXPECT_EQ(hex(wallet.entropy), "ba5821e8c356c05ba5f025d9532fe0f21f65d594");
    }
}

TEST(HDWallet, createFromMnemonicInvalid) {
    EXPECT_EXCEPTION(HDWallet("THIS IS AN INVALID MNEMONIC", password), "Invalid mnemonic");
    EXPECT_EXCEPTION(HDWallet("", password), "Invalid mnemonic");
}

TEST(HDWallet, createFromData) {
    {
        HDWallet wallet = HDWallet(parse_hex("ba5821e8c356c05ba5f025d9532fe0f21f65d594"), password);
        EXPECT_EQ(wallet.mnemonic, mnemonic1);
    }
}

TEST(HDWallet, createFromDataInvalid) {
    EXPECT_EXCEPTION(HDWallet(parse_hex(""), password), "Invalid mnemonic data");
    EXPECT_EXCEPTION(HDWallet(parse_hex("123456"), password), "Invalid mnemonic data");
}

TEST(HDWallet, privateKeyFromXPRV) {
    const std::string xprv = "xprv9yqEgpMG2KCjvotCxaiMkzmKJpDXz2xZi3yUe4XsURvo9DUbPySW1qRbdeDLiSxZt88hESHUhm2AAe2EqfWM9ucdQzH3xv1HoKoLDqHMK9n";
    auto privateKey = HDWallet::getPrivateKeyFromExtended(xprv, TWCoinTypeBitcoinCash, DerivationPath(TWPurposeBIP44, TWCoinTypeSlip44Id(TWCoinTypeBitcoinCash), 0, 0, 3));
    ASSERT_TRUE(privateKey);
    auto publicKey = privateKey->getPublicKey(TWPublicKeyTypeSECP256k1);
    auto address = Bitcoin::CashAddress(publicKey);

    EXPECT_EQ(hex(publicKey.bytes), "025108168f7e5aad52f7381c18d8f880744dbee21dc02c15abe512da0b1cca7e2f");
    EXPECT_EQ(address.string(), "bitcoincash:qp3y0dyg6ya8nt4n3algazn073egswkytqs00z7rz4");
}

TEST(HDWallet, privateKeyFromXPRV_Invalid) {
    const std::string xprv = "xprv9y0000";
    auto privateKey = HDWallet::getPrivateKeyFromExtended(xprv, TWCoinTypeBitcoinCash, DerivationPath(TWPurposeBIP44, TWCoinTypeSlip44Id(TWCoinTypeBitcoinCash), 0, 0, 3));
    ASSERT_FALSE(privateKey);
}

TEST(HDWallet, privateKeyFromXPRV_InvalidVersion) {
    {
        // Version bytes (first 4) are invalid, 0x00000000
        const std::string xprv = "11117pE7xwz2GARukXY8Vj2ge4ozfX4HLgy5ztnJXjr5btzJE8EbtPhZwrcPWAodW2aFeYiXkXjSxJYm5QrnhSKFXDgACcFdMqGns9VLqESCq3";
        auto privateKey = HDWallet::getPrivateKeyFromExtended(xprv, TWCoinTypeBitcoinCash, DerivationPath(TWPurposeBIP44, TWCoinTypeSlip44Id(TWCoinTypeBitcoinCash), 0, 0, 3));
        ASSERT_FALSE(privateKey);
    }
    {
        // Version bytes (first 4) are invalid, 0xdeadbeef
        const std::string xprv = "pGoh3VZXR4mTkT4bfqj4paog12KmHkAWkdLY8HNsZagD1ihVccygLr1ioLBhVQsny47uEh5swP3KScFc4JJrazx1Y7xvzmH2y5AseLgVMwomBTg2";
        auto privateKey = HDWallet::getPrivateKeyFromExtended(xprv, TWCoinTypeBitcoinCash, DerivationPath(TWPurposeBIP44, TWCoinTypeSlip44Id(TWCoinTypeBitcoinCash), 0, 0, 3));
        ASSERT_FALSE(privateKey);
    }
}

TEST(HDWallet, privateKeyFromExtended_InvalidCurve) {
    // invalid coin & curve, should fail
    const std::string xprv = "xprv9yqEgpMG2KCjvotCxaiMkzmKJpDXz2xZi3yUe4XsURvo9DUbPySW1qRbdeDLiSxZt88hESHUhm2AAe2EqfWM9ucdQzH3xv1HoKoLDqHMK9n";
    auto privateKey = HDWallet::getPrivateKeyFromExtended(xprv, TWCoinType(123456), DerivationPath(TWPurposeBIP44, 123456, 0, 0, 0));
    ASSERT_FALSE(privateKey);
}

TEST(HDWallet, privateKeyFromXPRV_Invalid45) {
    // 45th byte is not 0
    const std::string xprv = "xprv9yqEgpMG2KCjvotCxaiMkzmKJpDXz2xZi3yUe4XsURvo9DUbPySW1qRbhw2dJ8QexahgVSfkjxU4FgmN4GLGN3Ui8oLqC6433CeyPUNVHHh";
    auto privateKey = HDWallet::getPrivateKeyFromExtended(xprv, TWCoinTypeBitcoinCash, DerivationPath(TWPurposeBIP44, TWCoinTypeSlip44Id(TWCoinTypeBitcoinCash), 0, 0, 3));
    ASSERT_FALSE(privateKey);
}

TEST(HDWallet, privateKeyFromMptv) {
    const std::string mptv = "Mtpv7SkyM349Svcf1WiRtB5hC91ZZkVsGuv3kz1V7tThGxBFBzBLFnw6LpaSvwpHHuy8dAfMBqpBvaSAHzbffvhj2TwfojQxM7Ppm3CzW67AFL5";
    auto privateKey = HDWallet::getPrivateKeyFromExtended(mptv, TWCoinTypeBitcoinCash, DerivationPath(TWPurposeBIP44, TWCoinTypeSlip44Id(TWCoinTypeBitcoinCash), 0, 0, 4));
    auto publicKey = privateKey->getPublicKey(TWPublicKeyTypeSECP256k1);

    auto witness = Data{0x00, 0x14};
    auto keyHash = Hash::sha256ripemd(publicKey.bytes.data(), 33);
    witness.insert(witness.end(), keyHash.begin(), keyHash.end());

    auto prefix = Data{TW::p2shPrefix(TWCoinTypeLitecoin)};
    auto redeemScript = Hash::sha256ripemd(witness.data(), witness.size());
    prefix.insert(prefix.end(), redeemScript.begin(), redeemScript.end());

    auto address = Bitcoin::Address(prefix);

    EXPECT_EQ(hex(publicKey.bytes), "02c36f9c3051e9cfbb196ecc35311f3ad705ea6798ffbe6b039e70f6bd047e6f2c");
    EXPECT_EQ(address.string(), "MBzcCaoLk9626cLj2UVvcxs6nsVUi39zEy");
}

TEST(HDWallet, privateKeyFromZprv) {
    const std::string zprv = "zprvAdzGEQ44z4WPLNCRpDaup2RumWxLGgR8PQ9UVsSmJigXsHVDaHK1b6qGM2u9PmxB2Gx264ctAz4yRoN3Xwf1HZmKcn6vmjqwsawF4WqQjfd";
    auto privateKey = HDWallet::getPrivateKeyFromExtended(zprv, TWCoinTypeBitcoinCash, DerivationPath(TWPurposeBIP44, TWCoinTypeSlip44Id(TWCoinTypeBitcoin), 0, 0, 5));
    auto publicKey = privateKey->getPublicKey(TWPublicKeyTypeSECP256k1);
    auto address = Bitcoin::SegwitAddress(publicKey, 0, "bc");

    EXPECT_EQ(hex(publicKey.bytes), "022dc3f5a3fcfd2d1cc76d0cb386eaad0e30247ba729da0d8847a2713e444fdafa");
    EXPECT_EQ(address.string(), "bc1q5yyq60jepll68hds7exa7kpj20gsvdu0aztw5x");
}

TEST(HDWallet, privateKeyFromDGRV) {
    const std::string dgpv = "dgpv595jAJYGBLanByCJXRzrWBZFVXdNisfuPmKRDquCQcwBbwKbeR21AtkETf4EpjBsfsK3kDZgMqhcuky1B9PrT5nxiEcjghxpUVYviHXuCmc";
    auto privateKey = HDWallet::getPrivateKeyFromExtended(dgpv, TWCoinTypeDogecoin, DerivationPath(TWPurposeBIP44, TWCoinTypeSlip44Id(TWCoinTypeDogecoin), 0, 0, 1));
    auto publicKey = privateKey->getPublicKey(TWPublicKeyTypeSECP256k1);
    auto address = Bitcoin::Address(publicKey, TW::p2pkhPrefix(TWCoinTypeDogecoin));

    EXPECT_EQ(hex(publicKey.bytes), "03eb6bf281990ee074a39c71ed8ce78c486066ac433bcf066dd5eb08f87d3a6c34");
    EXPECT_EQ(address.string(), "D5taDndQJ1fDF3AM1yWavmJY2BgSi17CUv");
}

TEST(HDWallet, privateKeyFromXPRVForDGB) {
    const std::string xprvForDgb = "xprv9ynLofyuR3uCqCMJADwzBaPnXB53EVe5oLujvPfdvCxae3NzgEpYjZMgcUeS8EUeYfYVLG61ZgPXm9TZWiwBnLVCgd551vCwpXC19hX3mFJ";
    auto privateKey = HDWallet::getPrivateKeyFromExtended(xprvForDgb, TWCoinTypeDigiByte, DerivationPath(TWPurposeBIP44, TWCoinTypeSlip44Id(TWCoinTypeDigiByte), 0, 0, 1));
    auto publicKey = privateKey->getPublicKey(TWPublicKeyTypeSECP256k1);
    auto address = Bitcoin::Address(publicKey, TW::p2pkhPrefix(TWCoinTypeDigiByte));

    EXPECT_EQ(hex(publicKey.bytes), "03238a5c541c2cbbf769dbe0fb2a373c22db4da029370767fbe746d59da4de07f1");
    EXPECT_EQ(address.string(), "D9Gv7jWSVsS9Y5q98C79WyfEj6P2iM5Nzs");
}

TEST(HDWallet, DeriveWithLeadingZerosEth) {
    // Derivation test case with leading zeroes, see  https://blog.polychainlabs.com/bitcoin,/bip32,/bip39,/kdf/2021/05/17/inconsistent-bip32-derivations.html
    const auto mnemonic = "name dash bleak force moral disease shine response menu rescue more will";
    const auto derivationPath = "m/44'/60'";
    const auto coin = TWCoinTypeEthereum;
    auto wallet = HDWallet(mnemonic, "");
    const auto addr = Ethereum::Address(wallet.getKey(coin, DerivationPath(derivationPath)).getPublicKey(TW::publicKeyType(coin)));
    EXPECT_EQ(addr.string(), "0x0ba17e928471c64AaEaf3ABfB3900EF4c27b380D");
}

} // namespace
