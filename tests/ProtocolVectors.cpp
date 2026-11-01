// SPDX-License-Identifier: GPL-3.0-or-later
// Public synthetic review fixtures. Deterministic randomness exists only in this executable.
#include "core/Pairing.h"
#include "core/SecureTransport.h"
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <sodium.h>
#include <stdexcept>
using namespace aw;
namespace {
unsigned char nextScalar = 1;
const char* rngName() { return "AudioWire PUBLIC TEST FIXTURES ONLY"; }
uint32_t rngWord() { return nextScalar++; }
void rngBuffer(void* out, size_t size) {
    if (size != 32 || nextScalar == 0)
        std::abort(); // Never throw across the C RNG callback boundary.
    std::memset(out, 0, size);
    static_cast<unsigned char*>(out)[0] = nextScalar++;
}
randombytes_implementation fixtureRng{rngName, rngWord, nullptr, nullptr, rngBuffer, nullptr};
using Fields = std::map<std::string, std::string>;
void require(bool ok, const char* label) {
    if (!ok) throw std::runtime_error(label);
}
std::string encode(const Bytes& b) {
    std::string out(b.size() * 2 + 1, '\0');
    sodium_bin2hex(out.data(), out.size(), b.data(), b.size());
    out.pop_back();
    return out;
}
void append(Bytes& b, const Key& k) { b.insert(b.end(), k.begin(), k.end()); }
void field(Bytes& b, const std::string& s) {
    require(s.size() < 256, "fixture field length");
    b.push_back(static_cast<uint8_t>(s.size()));
    b.insert(b.end(), s.begin(), s.end());
}
Key parameter(const std::string& label) {
    std::array<unsigned char, 64> digest{};
    crypto_generichash(digest.data(), digest.size(),
        reinterpret_cast<const unsigned char*>(label.data()), label.size(), nullptr, 0);
    Key p{};
    require(crypto_core_ristretto255_from_hash(p.data(), digest.data()) == 0, "hash to group");
    return p;
}
Fields generate() {
    Fields result;
    const auto m = parameter("AudioWire v1 SPAKE2 Ristretto255 M");
    const auto n = parameter("AudioWire v1 SPAKE2 Ristretto255 N");
    result["M"] = hex(m);
    result["N"] = hex(n);
    const std::string pcId = "0011223344556677", phoneId = "8899aabbccddeeff";
    for (const auto& code : {std::string("012345"), std::string("999999")}) {
        const auto prefix = code + ".";
        nextScalar = 1;
        Pairing pc(Role::pc, code, pcId, phoneId), phone(Role::phone, code, pcId, phoneId);
        require(nextScalar == 3, "fixture scalar selection changed");
        auto pairKey = pc.finish(phone.message());
        require(pairKey == phone.finish(pc.message()), "two-party fixture key mismatch");
        Bytes context;
        field(context, "AudioWire v1 pairing"); field(context, pcId); field(context, phoneId);
        auto passwordInput = context;
        field(passwordInput, code);
        std::array<unsigned char, 64> digest{};
        crypto_generichash(digest.data(), digest.size(), passwordInput.data(), passwordInput.size(), nullptr, 0);
        Key password{}, x{}, y{}, base{}, mask{}, a{}, b{}, unmasked{}, shared{};
        x[0] = 1; y[0] = 2;
        crypto_core_ristretto255_scalar_reduce(password.data(), digest.data());
        // Separate, direct equations let reviewers compare intermediate values with Pairing.cpp.
        require(crypto_scalarmult_ristretto255_base(base.data(), x.data()) == 0, "xG");
        require(crypto_scalarmult_ristretto255(mask.data(), password.data(), m.data()) == 0, "wM");
        require(crypto_core_ristretto255_add(a.data(), base.data(), mask.data()) == 0, "X");
        require(crypto_scalarmult_ristretto255_base(base.data(), y.data()) == 0, "yG");
        require(crypto_scalarmult_ristretto255(mask.data(), password.data(), n.data()) == 0, "wN");
        require(crypto_core_ristretto255_add(b.data(), base.data(), mask.data()) == 0, "Y");
        require(a == pc.message() && b == phone.message(), "direct equation messages differ");
        require(crypto_core_ristretto255_sub(unmasked.data(), b.data(), mask.data()) == 0, "Y-wN");
        require(crypto_scalarmult_ristretto255(shared.data(), x.data(), unmasked.data()) == 0, "shared");
        auto transcript = context; append(transcript, a); append(transcript, b);
        auto input = transcript; append(input, shared);
        require(hash(input) == pairKey, "direct equation pair key differs");
        Key pcNonce{}, phoneNonce{};
        for (unsigned i = 0; i < 32; ++i) { pcNonce[i] = static_cast<unsigned char>(i); phoneNonce[i] = static_cast<unsigned char>(32 + i); }
        const auto keys = deriveSessionKeys(pairKey, pcNonce, phoneNonce, Role::pc);
        const auto reverse = deriveSessionKeys(pairKey, pcNonce, phoneNonce, Role::phone);
        require(keys.transmit == reverse.receive && keys.receive == reverse.transmit, "direction binding");
        result[prefix + "pc_id"] = pcId; result[prefix + "phone_id"] = phoneId;
        result[prefix + "x"] = hex(x); result[prefix + "y"] = hex(y);
        result[prefix + "context"] = encode(context); result[prefix + "w"] = hex(password);
        result[prefix + "X"] = hex(a); result[prefix + "Y"] = hex(b);
        result[prefix + "shared"] = hex(shared); result[prefix + "transcript"] = encode(transcript);
        result[prefix + "pair_key"] = hex(pairKey);
        result[prefix + "pc_confirmation"] = hex(pc.confirmation(pairKey, Role::pc));
        result[prefix + "phone_confirmation"] = hex(phone.confirmation(pairKey, Role::phone));
        result[prefix + "pc_nonce"] = hex(pcNonce); result[prefix + "phone_nonce"] = hex(phoneNonce);
        result[prefix + "pc_audio_tx"] = hex(keys.transmit); result[prefix + "phone_audio_tx"] = hex(keys.receive);
        SessionKeys control{authenticate(keys.transmit, "AudioWire v1 control", {}), authenticate(keys.receive, "AudioWire v1 control", {})};
        result[prefix + "pc_control_tx"] = hex(control.transmit); result[prefix + "phone_control_tx"] = hex(control.receive);
        Bytes resume(pcId.begin(), pcId.end()); resume.insert(resume.end(), phoneId.begin(), phoneId.end());
        append(resume, pcNonce); append(resume, phoneNonce);
        result[prefix + "resume_transcript"] = encode(resume);
        result[prefix + "pc_resume_proof"] = hex(authenticate(pairKey, "PC resumes", resume));
        result[prefix + "phone_resume_proof"] = hex(authenticate(pairKey, "Phone resumes", resume));
        SecureTransport audio(keys), controls(control);
        const Bytes payload{0, 1, 2, 3};
        result[prefix + "audio_counter0"] = encode(audio.seal(PacketKind::audio, payload.data(), payload.size()));
        result[prefix + "control_counter0"] = encode(controls.seal(PacketKind::control, payload.data(), payload.size()));
    }
    return result;
}
} // namespace
int main(int argc, char** argv) {
    try {
        initializeCrypto();
        require(randombytes_set_implementation(&fixtureRng) == 0, "fixture RNG setup");
        const auto actual = generate();
        if (argc == 3 && std::string(argv[1]) == "--write-public-fixtures") {
            std::ofstream out(argv[2], std::ios::binary);
            require(out.good(), "cannot create fixture file");
            out << "# PUBLIC SYNTHETIC TEST VALUES. NEVER use these keys or RNG in an application.\n";
            out << "# Candidate regression vectors, not independently certified cryptographic vectors.\n";
            for (const auto& item : actual) out << item.first << '=' << item.second << '\n';
            out.close(); require(out.good(), "fixture write failed");
            std::cout << "Wrote public synthetic protocol fixtures (values omitted).\n";
            return 0;
        }
        require(argc == 1, "unexpected vector-test arguments");
        std::ifstream in(AUDIOWIRE_VECTOR_FILE);
        require(in.good(), "missing protocol fixture file");
        Fields expected;
        for (std::string line; std::getline(in, line);) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty() || line[0] == '#') continue;
            const auto at = line.find('=');
            require(at != std::string::npos && expected.emplace(line.substr(0, at), line.substr(at + 1)).second, "malformed fixture");
        }
        require(actual == expected, "protocol fixture mismatch (values omitted)");
        std::cout << "PASS: " << actual.size() << " public protocol fields; two exchanges, direct equations, confirmations, resumption, session keys and encrypted packets\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << '\n'; return 1;
    }
}
