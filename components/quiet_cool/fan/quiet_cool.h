#pragma once

#include "esphome/components/fan/fan.h"
#include "esphome/core/component.h"
#include "esphome/components/spi/spi.h"
#include "esphome/components/button/button.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/preferences.h"
#include "quietcool.h"
#include <array>
#include <memory>
#include <string>
#include <vector>

namespace esphome {
    namespace quiet_cool {

        class QuietCoolFan :
	    public Component,
	    public fan::Fan,
	    public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_1KHZ>
	{
        public:

            void dump_config() override;
            fan::FanTraits get_traits() override;
            void setup() override;  // initialise radio
            void loop() override;
            float get_setup_priority() const override { return setup_priority::BUS; }
            void set_pins(uint8_t csn, uint8_t gdo0, uint8_t gdo2) {
                this->csn_pin_ = csn;
                this->gdo0_pin_ = gdo0;
                this->gdo2_pin_ = gdo2;
                this->pins_set_ = true;
            }
            void set_spi_pins(uint8_t sck, uint8_t miso, uint8_t mosi) {
                this->sck_pin_ = sck;
                this->miso_pin_ = miso;
                this->mosi_pin_ = mosi;
            }
	    void set_frequencies(float center_freq_mhz, float deviation_khz) {
		this->center_freq_mhz = center_freq_mhz;
		this->deviation_khz = deviation_khz;
	    }
            void set_speed_count(uint8_t speed_count) { this->speed_count_ = speed_count; }
            void set_learning_status_sensor(text_sensor::TextSensor *sensor) { this->learning_status_sensor_ = sensor; }
            void start_learning();
            bool is_learning() const { return this->learning_active_; }

        protected:
            void control(const fan::FanCall &call) override;
            void apply_received_command_(const QuietCoolCommand &command);
            QuietCoolSpeed to_radio_speed_(int speed) const;
            int from_radio_speed_(QuietCoolSpeed speed) const;
        private:
            std::unique_ptr<QuietCool> qc_;

            struct RemotePreference {
                uint8_t version;
                uint8_t id[7];
                uint8_t checksum;
            };
            static constexpr uint8_t REMOTE_PREF_VERSION = 1;
            // Allow time for two remote presses whose command transmission is delayed.
            static constexpr uint32_t LEARNING_TIMEOUT_MS = 60000;
            static constexpr uint32_t PRESS_GAP_MS = 700;
            static constexpr uint32_t LEARNING_PROGRESS_LOG_MS = 5000;
            ESPPreferenceObject remote_pref_;
            text_sensor::TextSensor *learning_status_sensor_{nullptr};

            uint8_t csn_pin_{};
            uint8_t gdo0_pin_{};
            uint8_t gdo2_pin_{};
            uint8_t sck_pin_{};
            uint8_t miso_pin_{};
            uint8_t mosi_pin_{};
	    uint8_t speed_count_{3};
	    float center_freq_mhz{433.897};
	    float deviation_khz{10};
            bool pins_set_{false};
            bool remote_timer_active_{false};
            uint32_t remote_off_at_{0};
            std::array<uint8_t, 7> remote_id_{};
            learning::Confirmation learning_confirmation_{};
            bool remote_id_configured_{false};
            bool has_remote_id_{false};
            bool learning_active_{false};
            uint32_t learning_started_at_{0};
            uint32_t learning_progress_log_at_{0};
            uint32_t learning_last_reported_edges_{0};
            uint32_t learning_valid_frames_{0};
            std::string learning_status_{"Not Configured"};
            void set_learning_status_(const char *status);
            bool save_remote_id_();
            static uint8_t remote_checksum_(const uint8_t *id);
            void finish_learning_(const std::array<uint8_t, 7> &id);
        public:
            void set_remote_id(const std::vector<uint8_t> &remote_id) {
                for (size_t i = 0; i < 7 && i < remote_id.size(); ++i) remote_id_[i] = remote_id[i];
                this->remote_id_configured_ = remote_id.size() == 7;
            }
        };

        class QuietCoolLearnButton : public button::Button {
         public:
            void set_parent(QuietCoolFan *parent) { this->parent_ = parent; }
         protected:
            void press_action() override;
         private:
            QuietCoolFan *parent_{nullptr};
        };

        class QuietCoolLearningStatus : public Component, public text_sensor::TextSensor {
         public:
            void setup() override {}
        };

    }  // namespace quiet_cool
}  // namespace esphome
