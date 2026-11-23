#include "quiet_cool.h"
#include "esphome/core/log.h"
#include "quietcool.h"
#include <algorithm>
#include <cstdio>

namespace esphome {
    namespace quiet_cool {
        
        static const char *TAG = "quiet_cool.fan";

        uint8_t QuietCoolFan::remote_checksum_(const uint8_t *id) {
            uint8_t checksum = 0xA7;
            for (size_t i = 0; i < 7; i++)
                checksum = static_cast<uint8_t>((checksum << 1) | (checksum >> 7)) ^ id[i];
            return checksum;
        }

        void QuietCoolFan::set_learning_status_(const char *status) {
            this->learning_status_ = status;
            this->publish_learning_status_();
        }

        void QuietCoolFan::publish_learning_status_() {
            if (this->learning_status_sensor_ != nullptr)
            {
                char remote_id[24];
                if (this->learning_active_ && this->learning_confirmation_.press_count != 0) {
                    const auto *id = this->learning_confirmation_.remote_id;
                    snprintf(remote_id, sizeof(remote_id), "%02X:%02X:%02X:%02X:%02X:%02X:%02X",
                             id[0], id[1], id[2], id[3], id[4], id[5], id[6]);
                } else if (this->has_remote_id_) {
                    snprintf(remote_id, sizeof(remote_id), "%02X:%02X:%02X:%02X:%02X:%02X:%02X",
                             this->remote_id_[0], this->remote_id_[1], this->remote_id_[2],
                             this->remote_id_[3], this->remote_id_[4], this->remote_id_[5],
                             this->remote_id_[6]);
                } else {
                    snprintf(remote_id, sizeof(remote_id), "not learned");
                }
                char details[128];
                snprintf(details, sizeof(details), "%s | ID %s | %.3f MHz | dev %.1f kHz",
                         this->learning_status_.c_str(), remote_id,
                         this->center_freq_mhz, this->deviation_khz);
                this->learning_status_sensor_->publish_state(details);
            }
        }

        bool QuietCoolFan::save_remote_id_() {
            RemotePreference saved{};
            saved.version = REMOTE_PREF_VERSION;
            std::copy(this->remote_id_.begin(), this->remote_id_.end(), saved.id);
            saved.checksum = remote_checksum_(saved.id);
            if (!this->remote_pref_.save(&saved) || !global_preferences->sync()) {
                ESP_LOGE(TAG, "Failed to persist learned remote ID");
                return false;
            }
            return true;
        }

        void QuietCoolFan::setup() {
            ESP_LOGI(TAG, "Firmware compiled %s %s (compiler local time)", __DATE__, __TIME__);
            if (!this->pins_set_) {
                ESP_LOGE(TAG, "QuietCool pins not configured via YAML; radio not initialised");
                this->set_learning_status_("Error");
                return;
            }

            this->remote_pref_ = global_preferences->make_preference<RemotePreference>(
                this->get_object_id_hash() ^ 0x51435246U, true);
            this->frequency_pref_ = global_preferences->make_preference<FrequencyPreference>(
                this->get_object_id_hash() ^ 0x46524551U, true);
            FrequencyPreference saved_frequency{};
            if (this->frequency_pref_.load(&saved_frequency) && saved_frequency.version == 1 &&
                saved_frequency.center_freq_mhz >= 433.0f && saved_frequency.center_freq_mhz <= 434.5f &&
                saved_frequency.deviation_khz >= 1.0f && saved_frequency.deviation_khz <= 100.0f) {
                if (!this->center_freq_fixed_)
                    this->center_freq_mhz = saved_frequency.center_freq_mhz;
                if (!this->deviation_fixed_)
                    this->deviation_khz = saved_frequency.deviation_khz;
                ESP_LOGI(TAG, "Loaded RF tuning from preferences: center %.3f MHz, deviation %.1f kHz",
                         this->center_freq_mhz, this->deviation_khz);
            }
            if (this->center_freq_number_ != nullptr)
                this->center_freq_number_->publish_state(this->center_freq_mhz);
            if (this->deviation_number_ != nullptr)
                this->deviation_number_->publish_state(this->deviation_khz);
            RemotePreference saved{};
            if (this->remote_pref_.load(&saved) && saved.version == REMOTE_PREF_VERSION &&
                saved.checksum == remote_checksum_(saved.id)) {
                std::copy(std::begin(saved.id), std::end(saved.id), this->remote_id_.begin());
                this->has_remote_id_ = true;
                ESP_LOGI(TAG, "Loaded learned remote ID from ESPHome preferences");
            } else {
                this->has_remote_id_ = this->remote_id_configured_;
            }
            if (this->has_remote_id_)
                ESP_LOGI(TAG, "Active QuietCool remote ID: %02X:%02X:%02X:%02X:%02X:%02X:%02X",
                         this->remote_id_[0], this->remote_id_[1], this->remote_id_[2],
                         this->remote_id_[3], this->remote_id_[4], this->remote_id_[5],
                         this->remote_id_[6]);
            else
                ESP_LOGI(TAG, "No QuietCool remote ID configured or learned yet");
            ESP_LOGI(TAG, "Active RF tuning: center %.3f MHz, deviation %.1f kHz",
                     this->center_freq_mhz, this->deviation_khz);
            this->publish_learning_status_();

            if (this->qc_ == nullptr) {
                this->qc_.reset(new QuietCool(this->csn_pin_, this->gdo0_pin_, this->gdo2_pin_,
                    this->sck_pin_, this->miso_pin_, this->mosi_pin_, remote_id_.data(),
                    center_freq_mhz, deviation_khz));
                this->qc_->setRxActivitySensor(this->rx_activity_sensor_);
            }

            if (!this->qc_->begin(this->has_remote_id_)) {
                this->set_learning_status_("Error");
                return;
            }
            this->qc_->setRemoteListeningEnabled(this->remote_listening_enabled_);
            if (this->remote_listener_switch_ != nullptr)
                this->remote_listener_switch_->publish_state(this->remote_listening_enabled_);
            this->set_learning_status_(this->has_remote_id_ ? "Ready" : "Not Configured");
            ESP_LOGD(TAG, "QuietCool initialized");
        }

        void QuietCoolFan::set_center_freq_mhz(float value) {
            if (value < 433.0f || value > 434.5f || this->qc_ == nullptr)
                return;
            this->center_freq_mhz = value;
            this->qc_->setFrequencies(this->center_freq_mhz, this->deviation_khz);
            FrequencyPreference saved{1, this->center_freq_mhz, this->deviation_khz};
            if (!this->frequency_pref_.save(&saved) || !global_preferences->sync())
                ESP_LOGE(TAG, "Failed to persist RF tuning settings");
            if (this->center_freq_number_ != nullptr)
                this->center_freq_number_->publish_state(value);
            this->learning_confirmation_.reset();
            this->learning_valid_frames_ = 0;
            this->publish_learning_status_();
            ESP_LOGI(TAG, "Updated RF center frequency to %.3f MHz", value);
        }

        void QuietCoolFan::set_deviation_khz(float value) {
            if (value < 1.0f || value > 100.0f || this->qc_ == nullptr)
                return;
            this->deviation_khz = value;
            this->qc_->setFrequencies(this->center_freq_mhz, this->deviation_khz);
            FrequencyPreference saved{1, this->center_freq_mhz, this->deviation_khz};
            if (!this->frequency_pref_.save(&saved) || !global_preferences->sync())
                ESP_LOGE(TAG, "Failed to persist RF tuning settings");
            if (this->deviation_number_ != nullptr)
                this->deviation_number_->publish_state(value);
            this->learning_confirmation_.reset();
            this->learning_valid_frames_ = 0;
            this->publish_learning_status_();
            ESP_LOGI(TAG, "Updated RF deviation to %.1f kHz", value);
        }

        fan::FanTraits QuietCoolFan::get_traits() {
            return fan::FanTraits(false, true, false, this->speed_count_);
        }

        QuietCoolSpeed QuietCoolFan::to_radio_speed_(int speed) const {
            if (speed <= 1)
                return QUIETCOOL_SPEED_LOW;
            if (this->speed_count_ == 2 || speed >= 3)
                return QUIETCOOL_SPEED_HIGH;
            return QUIETCOOL_SPEED_MEDIUM;
        }

        int QuietCoolFan::from_radio_speed_(QuietCoolSpeed speed) const {
            switch (speed) {
                case QUIETCOOL_SPEED_LOW:
                    return 1;
                case QUIETCOOL_SPEED_MEDIUM:
                    return this->speed_count_ == 3 ? 2 : 0;
                case QUIETCOOL_SPEED_HIGH:
                    return this->speed_count_;
                default:
                    return 0;
            }
        }

        void QuietCoolFan::loop() {
            if (this->qc_ != nullptr) {
                if (this->learning_active_) {
                    if (static_cast<uint32_t>(millis() - this->learning_started_at_) >= LEARNING_TIMEOUT_MS) {
                        uint32_t total_edges = 0;
                        uint16_t buffered_bits = 0;
                        this->qc_->getLearningCaptureStats(total_edges, buffered_bits);
                        this->learning_active_ = false;
                        this->learning_confirmation_.reset();
                        this->qc_->cancelLearning();
                        this->set_learning_status_("Timeout");
                        ESP_LOGW(TAG, "Learning timed out: %u GDO2 clock edges, %u buffered bits, %u valid frames decoded",
                                 static_cast<unsigned>(total_edges), static_cast<unsigned>(buffered_bits),
                                 static_cast<unsigned>(this->learning_valid_frames_));
                        this->qc_->logLearningCapture();
                    } else {
                        const uint32_t now = millis();
                        if (static_cast<uint32_t>(now - this->learning_progress_log_at_) >= LEARNING_PROGRESS_LOG_MS) {
                            uint32_t total_edges = 0;
                            uint16_t buffered_bits = 0;
                            this->qc_->getLearningCaptureStats(total_edges, buffered_bits);
                            if (this->learning_confirmation_.press_count == 0) {
                                ESP_LOGI(TAG, "Learning discovery: %u GDO2 edges (+%u), %u buffered bits, %u valid frames; waiting for remote activity",
                                         static_cast<unsigned>(total_edges),
                                         static_cast<unsigned>(total_edges - this->learning_last_reported_edges_),
                                         static_cast<unsigned>(buffered_bits),
                                         static_cast<unsigned>(this->learning_valid_frames_));
                            } else {
                                ESP_LOGI(TAG, "Learning discovery: %u GDO2 edges (+%u), %u buffered bits, %u valid frames; waiting for a second separate press",
                                         static_cast<unsigned>(total_edges),
                                         static_cast<unsigned>(total_edges - this->learning_last_reported_edges_),
                                         static_cast<unsigned>(buffered_bits),
                                         static_cast<unsigned>(this->learning_valid_frames_));
                            }
                            this->learning_progress_log_at_ = now;
                            this->learning_last_reported_edges_ = total_edges;
                        }
                        learning::Frame frame{};
                        if (this->qc_->captureLearningFrame(frame)) {
                            if (frame.command == 0x66) {
                                ESP_LOGI(TAG, "Recognized setup frame 0x66 for ID %02X:%02X:%02X:%02X:%02X:%02X:%02X (prefix errors=%u); ignored, waiting for a normal fan command",
                                         frame.remote_id[0], frame.remote_id[1], frame.remote_id[2],
                                         frame.remote_id[3], frame.remote_id[4], frame.remote_id[5],
                                         frame.remote_id[6], static_cast<unsigned>(frame.prefix_errors));
                            } else {
                                const uint32_t frame_time = millis();
                                const bool had_candidate = this->learning_confirmation_.press_count != 0;
                                const bool matching_candidate = std::equal(
                                    std::begin(frame.remote_id), std::end(frame.remote_id),
                                    this->learning_confirmation_.remote_id);
                                const uint32_t frame_gap = had_candidate && matching_candidate
                                    ? static_cast<uint32_t>(frame_time - this->learning_confirmation_.last_frame_at)
                                    : 0;
                                this->learning_valid_frames_++;
                                ESP_LOGD(TAG, "Decoded valid RF frame %u: ID %02X:%02X:%02X:%02X:%02X:%02X:%02X command=0x%02X prefix_errors=%u",
                                         static_cast<unsigned>(this->learning_valid_frames_),
                                         frame.remote_id[0], frame.remote_id[1], frame.remote_id[2],
                                         frame.remote_id[3], frame.remote_id[4], frame.remote_id[5],
                                         frame.remote_id[6], frame.command,
                                         static_cast<unsigned>(frame.prefix_errors));
                                if (this->learning_confirmation_.observe(frame.remote_id, frame_time, PRESS_GAP_MS)) {
                                    std::array<uint8_t, 7> confirmed_id{};
                                    std::copy(std::begin(this->learning_confirmation_.remote_id),
                                              std::end(this->learning_confirmation_.remote_id), confirmed_id.begin());
                                    ESP_LOGI(TAG, "Confirmed remote ID from two separate button presses (%u ms apart)",
                                             static_cast<unsigned>(frame_gap));
                                    this->finish_learning_(confirmed_id);
                                } else if (!had_candidate || !matching_candidate) {
                                    this->set_learning_status_("Confirming");
                                    ESP_LOGI(TAG, "First valid press captured; release the remote button, let its delayed transmission finish, then press it again");
                                } else if (frame_gap < PRESS_GAP_MS) {
                                    ESP_LOGD(TAG, "Repeated frame from the same press (%u ms gap); it does not count as the second press",
                                             static_cast<unsigned>(frame_gap));
                                }
                            }
                        }
                    }
                } else {
                    QuietCoolCommand command{};
                    if (this->qc_->receive(command))
                        this->apply_received_command_(command);
                }
            }

            if (this->remote_timer_active_ &&
                static_cast<int32_t>(millis() - this->remote_off_at_) >= 0) {
                this->remote_timer_active_ = false;
                this->state = false;
                ESP_LOGI(TAG, "Remote timer elapsed; publishing fan OFF");
                this->publish_state();
            }
        }

        void QuietCoolFan::control(const fan::FanCall &call) {
            if (this->learning_active_) {
                this->learning_active_ = false;
                this->learning_confirmation_.reset();
                this->qc_->cancelLearning();
                this->set_learning_status_(this->has_remote_id_ ? "Ready" : "Not Configured");
            }
            if (!this->has_remote_id_) {
                ESP_LOGW(TAG, "Ignoring fan control until a remote ID is configured or learned");
                return;
            }
            const int requested_speed = call.get_speed().value_or(this->speed > 0 ? this->speed : 1);
            const bool requested_state = call.get_state().value_or(this->state);
            ESP_LOGD(TAG, "Control called: state=%s, speed=%d%s",
                     call.get_state().has_value() ? (*call.get_state() ? "ON" : "OFF") : "<unchanged>",
                     requested_speed, call.get_speed().has_value() ? "" : " (unchanged)");

            QuietCoolSpeed qcspd = QUIETCOOL_SPEED_LOW;
            QuietCoolDuration qcdur;
            if (!requested_state) {
                // The captured physical remote uses B0 for OFF.
                qcspd = QUIETCOOL_SPEED_HIGH;
                qcdur = QUIETCOOL_DURATION_OFF;
            } else {
                qcdur = QUIETCOOL_DURATION_ON;
                qcspd = this->to_radio_speed_(requested_speed);
            }

            if (this->qc_ != nullptr)
                this->qc_->send(qcspd, qcdur);

            this->state = requested_state;
            if (requested_state)
                this->speed = std::max(1, std::min(static_cast<int>(this->speed_count_), requested_speed));
            this->remote_timer_active_ = false;

            ESP_LOGV(TAG, "Post-update internal state: state=%s speed=%d",
                     this->state ? "ON" : "OFF", this->speed);
            this->publish_state();
        }

        void QuietCoolFan::apply_received_command_(const QuietCoolCommand &command) {
            if (command.duration == QUIETCOOL_DURATION_OFF) {
                this->state = false;
                this->remote_timer_active_ = false;
            } else {
                const int received_speed = this->from_radio_speed_(command.speed);
                if (received_speed == 0) {
                    ESP_LOGW(TAG, "Ignoring MEDIUM command configured for a 2-speed fan");
                    return;
                }
                this->state = true;
                this->speed = received_speed;

                if (command.duration == QUIETCOOL_DURATION_ON) {
                    this->remote_timer_active_ = false;
                } else {
                    const uint32_t duration_ms =
                        static_cast<uint32_t>(command.duration) * 60UL * 60UL * 1000UL;
                    this->remote_off_at_ = millis() + duration_ms;
                    this->remote_timer_active_ = true;
                }
            }

            ESP_LOGI(TAG, "Publishing physical remote state: %s, speed %d, command 0x%02X",
                     this->state ? "ON" : "OFF", this->speed, command.code);
            this->publish_state();
        }

        void QuietCoolFan::dump_config() {
            LOG_FAN("", "QuietCool fan", this);
            ESP_LOGCONFIG(TAG, "  Speed count: %u", this->speed_count_);
        }

        void QuietCoolFan::start_learning() {
            if (this->learning_active_) {
                ESP_LOGW(TAG, "Remote learning is already active");
                return;
            }
            if (this->qc_ == nullptr || !this->qc_->startLearning()) {
                this->set_learning_status_("Error");
                ESP_LOGE(TAG, "Cannot start remote learning; CC1101 is not ready");
                return;
            }
            this->learning_active_ = true;
            this->learning_confirmation_.reset();
            this->learning_started_at_ = millis();
            this->learning_progress_log_at_ = this->learning_started_at_;
            this->learning_last_reported_edges_ = 0;
            this->learning_valid_frames_ = 0;
            this->set_learning_status_("Learning");
            char remote_id[24];
            if (this->has_remote_id_)
                snprintf(remote_id, sizeof(remote_id), "%02X:%02X:%02X:%02X:%02X:%02X:%02X",
                         this->remote_id_[0], this->remote_id_[1], this->remote_id_[2],
                         this->remote_id_[3], this->remote_id_[4], this->remote_id_[5],
                         this->remote_id_[6]);
            else
                snprintf(remote_id, sizeof(remote_id), "not learned");
            ESP_LOGI(TAG, "Learning started for 60 seconds: remote ID %s, center %.3f MHz, deviation %.1f kHz (GDO0 GPIO%u, GDO2 GPIO%u); press and release a remote button, allow its full delayed transmission to finish, then repeat",
                     remote_id,
                     this->center_freq_mhz, this->deviation_khz,
                     static_cast<unsigned>(this->gdo0_pin_), static_cast<unsigned>(this->gdo2_pin_));
        }

        void QuietCoolFan::send_test_command(uint8_t command_code) {
            if (this->learning_active_) {
                this->learning_active_ = false;
                this->learning_confirmation_.reset();
                this->qc_->cancelLearning();
                this->set_learning_status_(this->has_remote_id_ ? "Ready" : "Not Configured");
            }
            if (this->qc_ == nullptr || !this->has_remote_id_) {
                ESP_LOGW(TAG, "Cannot send test command 0x%02X without a configured or learned remote ID",
                         command_code);
                return;
            }
            ESP_LOGI(TAG, "Test button requested raw RF command 0x%02X", command_code);
            this->qc_->sendCommand(command_code);
        }

        void QuietCoolFan::set_remote_listening(bool enabled) {
            this->remote_listening_enabled_ = enabled;
            if (this->qc_ != nullptr)
                this->qc_->setRemoteListeningEnabled(enabled);
            ESP_LOGI(TAG, "Physical remote listening %s%s", enabled ? "enabled" : "disabled",
                     enabled ? "" : "; 0x66 status-query replies will still be accepted briefly");
        }

        void QuietCoolFan::finish_learning_(const std::array<uint8_t, 7> &id) {
            const auto previous_id = this->remote_id_;
            this->remote_id_ = id;
            if (!this->save_remote_id_()) {
                this->remote_id_ = previous_id;
                this->qc_->setRemoteId(this->remote_id_.data());
                this->qc_->cancelLearning();
                this->learning_active_ = false;
                this->set_learning_status_("Error");
                return;
            }
            this->has_remote_id_ = true;
            this->qc_->setRemoteId(this->remote_id_.data());
            this->qc_->cancelLearning();
            this->learning_active_ = false;
            this->set_learning_status_("Learned");
            ESP_LOGI(TAG, "Learned and stored QuietCool remote ID %02X:%02X:%02X:%02X:%02X:%02X:%02X",
                     id[0], id[1], id[2], id[3], id[4], id[5], id[6]);
        }

        void QuietCoolLearnButton::press_action() {
            if (this->parent_ != nullptr)
                this->parent_->start_learning();
        }

        void QuietCoolTestCommandButton::press_action() {
            if (this->parent_ != nullptr)
                this->parent_->send_test_command(this->command_);
        }

        void QuietCoolRemoteListenerSwitch::write_state(bool state) {
            if (this->parent_ != nullptr)
                this->parent_->set_remote_listening(state);
            this->publish_state(state);
        }

        void QuietCoolFrequencyNumber::control(float value) {
            if (this->parent_ == nullptr)
                return;
            if (this->center_frequency_)
                this->parent_->set_center_freq_mhz(value);
            else
                this->parent_->set_deviation_khz(value);
        }
    }  // namespace quiet_cool
}  // namespace esphome
