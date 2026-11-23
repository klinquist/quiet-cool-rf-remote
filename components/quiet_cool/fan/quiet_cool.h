#pragma once

#include "esphome/components/fan/fan.h"
#include "esphome/core/component.h"
#include "esphome/components/spi/spi.h"
#include "esphome/components/button/button.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/number/number.h"
#include "esphome/components/sensor/sensor.h"
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
            void set_frequency_fixed(bool center, bool deviation) {
                this->center_freq_fixed_ = center;
                this->deviation_fixed_ = deviation;
            }
            void set_center_freq_number(number::Number *number) { this->center_freq_number_ = number; }
            void set_deviation_number(number::Number *number) { this->deviation_number_ = number; }
            void set_center_freq_mhz(float value);
            void set_deviation_khz(float value);
            void set_speed_count(uint8_t speed_count) { this->speed_count_ = speed_count; }
            void set_learning_status_sensor(text_sensor::TextSensor *sensor) { this->learning_status_sensor_ = sensor; }
            void set_rx_activity_sensor(text_sensor::TextSensor *sensor) { this->rx_activity_sensor_ = sensor; }
            void set_remote_listener_switch(switch_::Switch *listener_switch) {
                this->remote_listener_switch_ = listener_switch;
            }
            void set_additional_remote_ids(const std::vector<uint8_t> &flat_ids) {
                this->additional_remote_ids_.clear();
                for (size_t offset = 0; offset + 7 <= flat_ids.size(); offset += 7) {
                    std::array<uint8_t, 7> id{};
                    for (size_t i = 0; i < id.size(); i++)
                        id[i] = flat_ids[offset + i];
                    this->additional_remote_ids_.push_back(id);
                }
            }
            void set_status_polling_config(bool enabled, uint16_t interval_seconds) {
                this->status_polling_enabled_ = enabled;
                this->status_poll_interval_seconds_ = interval_seconds;
            }
            void set_status_polling_switch(switch_::Switch *polling_switch) {
                this->status_polling_switch_ = polling_switch;
            }
            void set_polling_interval_number(number::Number *number) {
                this->polling_interval_number_ = number;
            }
            bool set_status_polling_enabled(bool enabled);
            void set_status_poll_interval(float interval_seconds);
            void set_timer_remaining_sensor(sensor::Sensor *sensor) { this->timer_remaining_sensor_ = sensor; }
            void set_remote_listening(bool enabled);
            void set_timer_duration(size_t option_index);
            void start_learning();
            void send_test_command(uint8_t command_code);
            bool is_learning() const { return this->learning_active_; }

        protected:
            void control(const fan::FanCall &call) override;
            void apply_received_command_(const QuietCoolCommand &command);
            void publish_timer_remaining_();
            void start_timer_(QuietCoolDuration duration);
            void send_status_query_();
            QuietCoolSpeed to_radio_speed_(int speed) const;
            int from_radio_speed_(QuietCoolSpeed speed) const;
        private:
            std::unique_ptr<QuietCool> qc_;

            struct RemotePreference {
                uint8_t version;
                uint8_t id[7];
                uint8_t checksum;
            };
            struct FrequencyPreference {
                uint8_t version;
                float center_freq_mhz;
                float deviation_khz;
            };
            static constexpr uint8_t REMOTE_PREF_VERSION = 1;
            // Allow time for two remote presses whose command transmission is delayed.
            static constexpr uint32_t LEARNING_TIMEOUT_MS = 60000;
            static constexpr uint32_t PRESS_GAP_MS = 700;
            static constexpr uint32_t LEARNING_PROGRESS_LOG_MS = 5000;
            ESPPreferenceObject remote_pref_;
            ESPPreferenceObject frequency_pref_;
            number::Number *center_freq_number_{nullptr};
            number::Number *deviation_number_{nullptr};
            text_sensor::TextSensor *learning_status_sensor_{nullptr};
            text_sensor::TextSensor *rx_activity_sensor_{nullptr};
            switch_::Switch *remote_listener_switch_{nullptr};
            switch_::Switch *status_polling_switch_{nullptr};
            sensor::Sensor *timer_remaining_sensor_{nullptr};
            number::Number *polling_interval_number_{nullptr};

            uint8_t csn_pin_{};
            uint8_t gdo0_pin_{};
            uint8_t gdo2_pin_{};
            uint8_t sck_pin_{};
            uint8_t miso_pin_{};
            uint8_t mosi_pin_{};
	    uint8_t speed_count_{3};
	    float center_freq_mhz{433.897};
	    float deviation_khz{10};
            bool center_freq_fixed_{false};
            bool deviation_fixed_{false};
            bool pins_set_{false};
            bool remote_timer_active_{false};
            uint32_t remote_off_at_{0};
            uint32_t last_timer_sensor_publish_at_{0};
            QuietCoolDuration configured_timer_duration_{QUIETCOOL_DURATION_ON};
            std::array<uint8_t, 7> remote_id_{};
            std::vector<std::array<uint8_t, 7>> additional_remote_ids_{};
            learning::Confirmation learning_confirmation_{};
            bool remote_id_configured_{false};
            bool has_remote_id_{false};
            bool remote_listening_enabled_{true};
            bool status_polling_enabled_{false};
            uint16_t status_poll_interval_seconds_{30};
            uint32_t last_status_poll_at_{0};
            bool learning_active_{false};
            uint32_t learning_started_at_{0};
            uint32_t learning_progress_log_at_{0};
            uint32_t learning_last_reported_edges_{0};
            uint32_t learning_valid_frames_{0};
            std::string learning_status_{"Not Configured"};
            void set_learning_status_(const char *status);
            void publish_learning_status_();
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

        class QuietCoolTestCommandButton : public button::Button {
         public:
            void set_parent(QuietCoolFan *parent) { this->parent_ = parent; }
            void set_command(uint8_t command) { this->command_ = command; }
         protected:
            void press_action() override;
         private:
            QuietCoolFan *parent_{nullptr};
            uint8_t command_{0x66};
        };

        class QuietCoolRemoteListenerSwitch : public switch_::Switch {
         public:
            void set_parent(QuietCoolFan *parent) { this->parent_ = parent; }
         protected:
            void write_state(bool state) override;
         private:
            QuietCoolFan *parent_{nullptr};
        };

        class QuietCoolStatusPollingSwitch : public switch_::Switch {
         public:
            void set_parent(QuietCoolFan *parent) { this->parent_ = parent; }
         protected:
            void write_state(bool state) override;
         private:
            QuietCoolFan *parent_{nullptr};
        };

        class QuietCoolPollingIntervalNumber : public number::Number {
         public:
            void set_parent(QuietCoolFan *parent) { this->parent_ = parent; }
         protected:
            void control(float value) override;
         private:
            QuietCoolFan *parent_{nullptr};
        };

        class QuietCoolTimerButton : public button::Button {
         public:
            void set_parent(QuietCoolFan *parent) { this->parent_ = parent; }
            void set_option_index(size_t option_index) { this->option_index_ = option_index; }
         protected:
            void press_action() override;
         private:
            QuietCoolFan *parent_{nullptr};
            size_t option_index_{0};
        };

        class QuietCoolLearningStatus : public Component, public text_sensor::TextSensor {
         public:
            void setup() override {}
        };

        class QuietCoolFrequencyNumber : public number::Number {
         public:
            void set_parent(QuietCoolFan *parent, bool center_frequency) {
                this->parent_ = parent;
                this->center_frequency_ = center_frequency;
            }
         protected:
            void control(float value) override;
         private:
            QuietCoolFan *parent_{nullptr};
            bool center_frequency_{false};
        };

    }  // namespace quiet_cool
}  // namespace esphome
