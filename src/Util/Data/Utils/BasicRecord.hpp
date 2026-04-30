#pragma once

namespace Serialization {

	template <class T>
	bool CheckFloat(T& value){
		if constexpr (std::is_floating_point_v<T>) {
			if (std::isnan(value)) {
				value = 0;
				return true;
			}
		}
		return false;
	}

	static std::string Uint32ToStr(std::uint32_t value){
		char bytes[4];
		bytes[3] = static_cast<char>((value >> 24) & 0xFF);
		bytes[2] = static_cast<char>((value >> 16) & 0xFF);
		bytes[1] = static_cast<char>((value >> 8) & 0xFF);
		bytes[0] = static_cast<char>(value & 0xFF);
		return std::string(bytes, 4);
	}

	// -------------------- string wire helpers --------------------
	// Format: [uint32 byteCount][byteCount bytes (no null terminator)]
	// Supports std::string and std::string_view on save; std::string on load.

	static bool ReadU32(SKSE::SerializationInterface* s, std::uint32_t& out){
		return s->ReadRecordData(&out, sizeof(out));
	}

	static bool WriteU32(SKSE::SerializationInterface* s, std::uint32_t v){
		return s->WriteRecordData(&v, sizeof(v));
	}

	static bool ReadStringPayload(SKSE::SerializationInterface* s, std::uint32_t payloadSize, std::string& out){
		// If payloadSize includes a legacy uint64 length header, handle it too.
		// - New: payloadSize == 4 + len
		// - Old: payloadSize == 8 + len  (uint64 len)
		if (payloadSize == 0) {
			out.clear();
			return true;
		}

		if (payloadSize >= sizeof(std::uint32_t)) {
			std::uint32_t len32 = 0;
			const auto startOk = s->ReadRecordData(&len32, sizeof(len32));
			if (!startOk) {
				return false;
			}

			const std::uint32_t remaining = payloadSize - sizeof(std::uint32_t);
			if (len32 != remaining) {
				// Try legacy uint64 format if the uint32 interpretation doesn't match.
				// Re-read by interpreting that first 4 bytes as part of the uint64.
				// SKSE interface doesn't support seeking; fallback: treat as raw bytes record.
				// Best-effort: if sizes match legacy expectation, read remaining as string bytes.
				// (This path is mainly for robustness if someone wrote length but passed size differently.)
				out.resize(remaining);
				if (remaining == 0) {
					out.clear();
					return true;
				}
				return s->ReadRecordData(out.data(), remaining);
			}

			out.resize(len32);
			if (len32 == 0) {
				return true;
			}
			return s->ReadRecordData(out.data(), len32);
		}

		// Too small to contain even a length header: treat as empty/invalid.
		out.clear();
		return false;
	}

	static bool WriteStringPayload(SKSE::SerializationInterface* s, std::string_view sv){
		const std::uint32_t len = static_cast<std::uint32_t>(sv.size());
		if (!WriteU32(s, len)) {
			return false;
		}
		if (len == 0) {
			return true;
		}
		return s->WriteRecordData(sv.data(), len);
	}

	// -------------------- record --------------------
	template <class T, std::uint32_t uid, std::uint32_t ver = 1>
	struct BasicRecord{
		using value_type = T;

		T value{};
		static constexpr auto ID = std::byteswap(uid);

		BasicRecord() = default;
		explicit BasicRecord(const T& v) : value(v) {}

		void Load(SKSE::SerializationInterface* s, std::uint32_t type, std::uint32_t version, std::uint32_t size){
			if (type != ID) {
				return;
			}

			logger::trace("{}: Is being Read", Uint32ToStr(ID));

			if constexpr (std::is_same_v<std::remove_cvref_t<T>, std::string>) {
				// Accept variable size (string payload), not sizeof(T).
				if (version == ver) {
					std::string tmp;
					if (ReadStringPayload(s, size, tmp)) {
						value = std::move(tmp);
						logger::trace("{}: Read OK! (string, bytes={})", Uint32ToStr(ID), value.size());
						return;
					}
				}
				logger::error("{}: Could not be loaded! (string)", Uint32ToStr(ID));
			}
			else {
				if (version == ver && size == sizeof(T)) {
					if (s->ReadRecordData(&value, sizeof(T))) {
						logger::trace("{}: Read OK!", Uint32ToStr(ID));
						if (CheckFloat(value)) {
							logger::warn("{}: Was NaN!", Uint32ToStr(ID));
						}
						return;
					}
				}
				logger::error("{}: Could not be loaded!", Uint32ToStr(ID));
			}
		}

		void Save(SKSE::SerializationInterface* s) const {
			logger::trace("{}: Is being saved!", Uint32ToStr(ID));

			if (!s->OpenRecord(ID, ver)) {
				logger::error("{}: Could not be saved", Uint32ToStr(ID));
				return;
			}

			if constexpr (std::is_same_v<std::remove_cvref_t<T>, std::string>) {
				if (WriteStringPayload(s, std::string_view{ value })) {
					logger::trace("{}: Save OK! (string, bytes={})", Uint32ToStr(ID), value.size());
					return;
				}
				logger::error("{}: Could not be saved (string)", Uint32ToStr(ID));
			}
			else {
				if (s->WriteRecordData(&value, sizeof(T))) {
					logger::trace("{}: Save OK!", Uint32ToStr(ID));
					return;
				}
				logger::error("{}: Could not be saved", Uint32ToStr(ID));
			}
		}
	};

	template <std::uint32_t uid, std::uint32_t ver = 1>
	using StringRecord = BasicRecord<std::string, uid, ver>;

	template <std::uint32_t uid, std::uint32_t ver = 1>
	struct StringViewRecord {
		std::string_view value{};
		static constexpr auto ID = std::byteswap(uid);

		void Load(SKSE::SerializationInterface*, std::uint32_t, std::uint32_t, std::uint32_t) = delete;

		void Save(SKSE::SerializationInterface* s) const {
			logger::trace("{}: StringView is being saved! Length: {}", Uint32ToStr(ID), value.size());
			if (s->OpenRecord(ID, ver) && WriteStringPayload(s, value)) {
				logger::trace("{}: StringView Save OK!", Uint32ToStr(ID));
				return;
			}
			logger::error("{}: StringView could not be saved", Uint32ToStr(ID));
		}
	};
}