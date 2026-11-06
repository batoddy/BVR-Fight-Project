#pragma once

#include "model/aircraft.hpp"
#include "model/flight_command.hpp"
#include "model/phase.hpp"

// =============================================================================
// ORTAK  —  bütün manevraların arayüzü ve uçurma yardımcısı
// =============================================================================

enum class ManeuverStatus
{
    Running,   // sürüyor
    Done,      // kendi hedefine ulaştı (yalnız kendi içinde biten manevralar)
};

// Manevra yalnız "bu durumda nasıl uçarım" sorusunu cevaplar; hangisinin çalışacağını otopilot seçer.
// Gaz ve g sınırı manevranın içindedir: her manevra kendi THROTTLE (throttle::) ve G_LIMIT değerini yazar.
class Maneuver
{
public:
    virtual ~Maneuver() = default;

    virtual Phase phase() const = 0;                                   // görüntüleyicide ve CSV'de görünen ad
    virtual void enter(const Aircraft& own) { (void)own; }             // manevraya geçilince bir kez: girişe bağlı hedefleri kilitler
    virtual FlightCommand update(const Aircraft& own) = 0;             // her step: own.command'dan başlayıp yeni komutu döner
    virtual ManeuverStatus status(const Aircraft& own) const           // Out, Recommit: hedefe varınca Done; diğerleri hep Running
    {
        (void)own;
        return ManeuverStatus::Running;
    }
};

// Manevra yönü, havacılık terimiyle: uçağın döndüğü taraf.
enum class ManeuverSide
{
    Right,   // uçak sağa döner. Crank: düşman burnun SOLUNDA kalır (ATA −). Recommit: SAĞINDA kalır (ATA +).
    Left,    // uçak sola döner. Crank: düşman burnun SAĞINDA kalır (ATA +). Recommit: SOLUNDA kalır (ATA −).
};

// =============================================================================
// HOLD  —  şu anki yön ve irtifayla, seyir gazıyla devam
// =============================================================================

// NOT: her step ölçülen yön ve irtifayı hedef yapar; irtifa kayarsa düzeltilmez. Girişte kilitlemek ayrı bir karar.
class Hold : public Maneuver
{
public:
    Phase phase() const override { return Phase::Hold; }
    FlightCommand update(const Aircraft& own) override;   // yön ve irtifa = şu anki ölçüm; g sınırı yazmaz (Python varsayılanı)

private:
    static constexpr double THROTTLE = throttle::DEFAULT;
};

// =============================================================================
// OUT  —  düşmana kuyruk dön, alçal, tam art yakıcı; düşman arkada kalınca biter
// =============================================================================

class Out : public Maneuver
{
public:
    Phase phase() const override { return Phase::Out; }
    void enter(const Aircraft& own) override;                     // hedef irtifayı kilitler: DESCEND_FT alçal, FLOOR_FT altına inme
    FlightCommand update(const Aircraft& own) override;           // yön = bearing − 180 + gecikme payı, irtifa = kilitli hedef, out g'si
    ManeuverStatus status(const Aircraft& own) const override;    // |ATA| > DONE_ATA_DEG: düşman arkada, Done

private:
    static constexpr double DESCEND_FT = 3300.0;                  // ft, out'ta ne kadar alçalınacak
    static constexpr double FLOOR_FT = 10000.0;                   // ft, out bunun altına indirmez
    static constexpr double THROTTLE = throttle::MAX_THRUST;
    static constexpr double EXIT_THROTTLE = throttle::DEFAULT;    // Done olunca: sonraki manevraya art yakıcı kalmaz
    static constexpr double DONE_ATA_DEG = 177.0;                 // derece, |ATA| bunu geçince düşman arkada

    double target_altitude_ft_ = 0;                               // ft, enter()'da bir kez hesaplanır
};

// =============================================================================
// COMMIT  —  düşmana doğru düz uç (saf takip)
// =============================================================================

class Commit : public Maneuver
{
public:
    Phase phase() const override { return Phase::Commit; }
    FlightCommand update(const Aircraft& own) override;   // yön = düşmanın bearing'i; irtifa aynı

private:
    static constexpr double THROTTLE = throttle::DEFAULT;
    static constexpr double G_LIMIT = 4.0;                // g, yumuşak dönüş: hız ve irtifa korunur
};

// =============================================================================
// RECOMMIT  —  düşmana dönüp atış açısına gel
// =============================================================================

// Hedef tam atış açısı (30°) olsaydı yön hatası küçüldükçe dönüş yumuşar, 40°'den 30°'ye iniş uzardı.
// Bu yüzden ilk varışa kadar hedef TURN_INSIDE_DEG içeride (20°) tutulur, varınca 30°'ye alınır ve orada kalınır.
class Recommit : public Maneuver
{
public:
    Phase phase() const override { return Phase::Recommit; }
    void enter(const Aircraft& own) override;                     // taraf: düşman sağımdaysa Right, solumdaysa Left; varış bayrağı false
    FlightCommand update(const Aircraft& own) override;           // yön = bearing ∓ hedef ATA + gecikme payı; irtifa aynı, g out_g
    ManeuverStatus status(const Aircraft& own) const override;    // |ATA| ≤ recommit_fire_ata_deg + DONE_TOLERANCE_DEG: Done
    ManeuverSide side() const { return side_; }                   // girişte seçilen taraf

private:
    static constexpr double TURN_INSIDE_DEG = 10.0;               // derece, varana kadar hedefin atış açısından içeride tutulduğu açı
    static constexpr double DONE_TOLERANCE_DEG = 1.0;             // derece, |ATA| atış açısını bu kadar aşsa da varılmış sayılır
    static constexpr double THROTTLE = throttle::MAX_THRUST;

    ManeuverSide side_ = ManeuverSide::Left;                      // enter()'da seçilir
    bool has_reached_ = false;                                    // atış açısına bir kez varıldı mı
};

// =============================================================================
// CRANK  —  düşmanı radar gimbal sınırının biraz içinde tutarak yanal kaçış
// =============================================================================

// Taraf girişte düşmanın gidişine göre seçilir (benim burnuma göre değil) ve crank boyunca sabit kalır.
class Crank : public Maneuver
{
public:
    Phase phase() const override { return Phase::Crank; }
    void enter(const Aircraft& own) override;                 // taraf: TA ≥ 0 (düşmanın burnunun sağındayım) ise Left, değilse Right
    FlightCommand update(const Aircraft& own) override;       // yön = bearing ± (gimbal − OFFSET_DEG) + gecikme payı; irtifa aynı
    bool in_radar_sight(const Aircraft& own) const;           // düşman radarın görüş konisinde mi (|ATA| < gimbal)
    ManeuverSide side() const { return side_; }               // girişte seçilen taraf

private:
    static constexpr double OFFSET_DEG = 3.0;                 // derece, gimbal sınırından içeri pay
    static constexpr double THROTTLE = throttle::IDLE;
    static constexpr double G_LIMIT = 3.0;                    // g, yumuşak dönüş (flight_control: 3 g crank 50° ≈ 14.5 s)

    ManeuverSide side_ = ManeuverSide::Left;                  // enter()'da seçilir
};

// =============================================================================
// BEAM  —  düşman uçağa dik uç (3/9 hattı)
// =============================================================================

// Taraf girişte burnuma göre seçilir ve sabit kalır; dönüş hep 90°'den kısa. Füzeye göre olanı Notch.
class Beam : public Maneuver
{
public:
    Phase phase() const override { return Phase::Beam; }
    void enter(const Aircraft& own) override;                 // taraf: düşman sağımdaysa (ATA > 0) Left, solumdaysa Right
    FlightCommand update(const Aircraft& own) override;       // yön = bearing − 90 (Left) / + 90 (Right) + gecikme payı; irtifa aynı
    ManeuverSide side() const { return side_; }               // girişte seçilen taraf

private:
    static constexpr double THROTTLE = throttle::DEFAULT;
    static constexpr double G_LIMIT = 4.0;                    // g, yumuşak dönüş

    ManeuverSide side_ = ManeuverSide::Left;                  // enter()'da seçilir
};

// =============================================================================
// NOTCH  —  füzeye dik uç (İSKELET)
// =============================================================================

// Beam'in füzeye göre olanı: bana gelen füzenin (own.picture.threat) 3/9 hattı.
class Notch : public Maneuver
{
public:
    Phase phase() const override { return Phase::Notch; }
    FlightCommand update(const Aircraft& own) override;   // örnek: yön = füzenin bearing'i ∓ 90; füze yoksa yön değişmez

private:
    static constexpr double THROTTLE = throttle::DEFAULT;
};

// =============================================================================
// DRAG  —  füzeye sırt dön (İSKELET)
// =============================================================================

// Out'un füzeye göre olanı: füzeyi tam arkaya al, kapanma hızını düşür.
class Drag : public Maneuver
{
public:
    Phase phase() const override { return Phase::Drag; }
    FlightCommand update(const Aircraft& own) override;   // örnek: yön = füzenin bearing'i − 180; füze yoksa yön değişmez

private:
    static constexpr double THROTTLE = throttle::MRT;     // P-1290: drag askeri güçle
};

// =============================================================================
// OFFSET  —  düşman yönünden verilen açı kadar yana uç (İSKELET)
// =============================================================================

// Açı ve taraf taktikten gelir: maneuvers.offset.set(40.0, ManeuverSide::Left) sonra fly.
class Offset : public Maneuver
{
public:
    Phase phase() const override { return Phase::Offset; }
    void set(double offset_deg, ManeuverSide side);       // fly'dan önce çağrılır
    FlightCommand update(const Aircraft& own) override;   // örnek: yön = bearing ± offset_deg_ (Right +, Left −)

private:
    static constexpr double THROTTLE = throttle::DEFAULT;

    double offset_deg_ = 0;                               // derece, burnun düşman yönünden sapması
    ManeuverSide side_ = ManeuverSide::Left;
};

// =============================================================================
// FLANK  —  düşmanın burnuna göre yan açıda kal (İSKELET)
// =============================================================================

// Aralık condition::Aspect::Flank'ten (30-70°). Açık soru: ta_deg mi (düşmanın burnu) ata_deg mi (benim burnum).
class Flank : public Maneuver
{
public:
    Phase phase() const override { return Phase::Flank; }
    FlightCommand update(const Aircraft& own) override;           // örnek: |ta| hedefin altındaysa düşmanın burnundan uzaklaş
    ManeuverStatus status(const Aircraft& own) const override;    // örnek: |ta| hedef aralığa girince Done

private:
    static constexpr double TARGET_TA_DEG = 50.0;                 // derece, Flank aralığının ortası
    static constexpr double THROTTLE = throttle::DEFAULT;
};

// =============================================================================
// COLLISION  —  LOS hızını sıfırla: çarpışma rotası (İSKELET)
// =============================================================================

// Bearing değişmiyorsa iki uçak aynı noktaya varır; en kısa yoldan kesişme.
class Collision : public Maneuver
{
public:
    Phase phase() const override { return Phase::Collision; }
    FlightCommand update(const Aircraft& own) override;   // örnek: yön = bearing + KP × los_rate_deg_s

private:
    static constexpr double THROTTLE = throttle::DEFAULT;
};

// =============================================================================
// FLY HEADING  —  verilen sabit yöne uç (İSKELET)
// =============================================================================

// Flow cold, Reset ve Departure için: yön taktikten gelir.
class FlyHeading : public Maneuver
{
public:
    Phase phase() const override { return Phase::FlyHeading; }
    void set(double heading_deg);                         // fly'dan önce çağrılır
    FlightCommand update(const Aircraft& own) override;   // örnek: yön = heading_deg_; irtifa aynı

private:
    static constexpr double THROTTLE = throttle::DEFAULT;

    double heading_deg_ = 0;                              // derece ±180
};

// =============================================================================
// SNAKE  —  periyodik sağ-sol (İSKELET)
// =============================================================================

// Zamana bağlı: saat World::sim_time_s'te var ama otopilota iletilmiyor; iletilince kullanılır.
class Snake : public Maneuver
{
public:
    Phase phase() const override { return Phase::Snake; }
    void enter(const Aircraft& own) override;             // örnek: girişteki yönü ve başlangıç zamanını kilitler
    FlightCommand update(const Aircraft& own) override;   // örnek: yön = ana yön ± AMPLITUDE_DEG, her HALF_PERIOD_S'de taraf değişir

private:
    static constexpr double AMPLITUDE_DEG = 30.0;         // derece, ana yönden sapma
    static constexpr double HALF_PERIOD_S = 10.0;         // s, bir tarafta kalma süresi
    static constexpr double THROTTLE = throttle::DEFAULT;

    double base_heading_deg_ = 0;                         // derece, enter()'da kilitlenir
};

// =============================================================================
// PURSUIT  —  pure, lag, lead takip (İSKELET)
// =============================================================================

enum class PursuitMode
{
    Pure,   // burun düşmanda; bugünkü Commit ile aynı
    Lag,    // burun düşmanın gerisinde: menzil yavaş kapanır, aşma riski az
    Lead,   // burun düşmanın önünde: menzil hızlı kapanır
};

// Tek sınıf, üç mod: maneuvers.pure_pursuit, lag_pursuit, lead_pursuit.
class Pursuit : public Maneuver
{
public:
    explicit Pursuit(PursuitMode mode) : mode_(mode) {}

    Phase phase() const override;                         // moda göre PurePursuit, LagPursuit, LeadPursuit
    FlightCommand update(const Aircraft& own) override;   // örnek: Pure bearing; Lag bearing ∓ LAG_DEG; Lead bearing ± öncü açı

private:
    static constexpr double LAG_DEG = 15.0;               // derece, Lag'de burnun düşmanın gerisinde kalacağı açı
    static constexpr double THROTTLE = throttle::DEFAULT;

    PursuitMode mode_;
};

// =============================================================================
// MANEUVERS  —  bir otopilotun manevra nesneleri
// =============================================================================

// Yeni manevra: sınıfını yukarıya kendi banner'ıyla yaz, buraya bir satır ekle.
class Maneuvers
{
public:
    void fly(Aircraft& own, Maneuver& maneuver);   // manevra değiştiyse enter() bir kez; sonra fazı ve komutu yazar

    Hold hold;                                     // rotayı koru
    Out out;                                       // kaç
    Commit commit;                                 // düşmana doğru düz uç
    Recommit recommit;                             // atış açısına dön
    Crank crank;                                   // radar sınırında yanal kaçış
    Beam beam;                                     // düşmana dik uç

    // İskelet: gövdeleri henüz yalnız gazı yazar.
    Notch notch;                                   // füzeye dik uç
    Drag drag;                                     // füzeye sırt dön
    Offset offset;                                 // düşman yönünden açı kadar yana
    Flank flank;                                   // düşmanın burnuna göre yan açı
    Collision collision;                           // çarpışma rotası
    FlyHeading fly_heading;                        // sabit yön
    Snake snake;                                   // periyodik sağ-sol
    Pursuit pure_pursuit{PursuitMode::Pure};       // burun düşmanda
    Pursuit lag_pursuit{PursuitMode::Lag};         // burun düşmanın gerisinde
    Pursuit lead_pursuit{PursuitMode::Lead};       // burun düşmanın önünde

private:
    Maneuver* active_ = nullptr;                   // son uçurulan manevra; ilk fly() da giriş sayılır
};
