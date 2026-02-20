#include "views/BatchEditDialog.h"

#include "services/Database.h"
#include "style/Theme.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QStyle>

BatchEditDialog::BatchEditDialog(const QVector<Track>& tracks, Database* db,
                                 QWidget* parent)
    : QDialog(parent)
    , m_tracks(tracks)
    , m_db(db)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    setAttribute(Qt::WA_StyledBackground, true);
    setFixedSize(480, 420);
    setObjectName("batchEditDialog");
    setStyleSheet(
        QString("#batchEditDialog { background: %1; border: 1px solid %2; }")
            .arg(Theme::Color::Bg1, Theme::Color::Border));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(Theme::Layout::PadXl, Theme::Layout::PadXl,
                             Theme::Layout::PadXl, Theme::Layout::PadLg);
    root->setSpacing(Theme::Layout::GapMd);

    // ── Header ──────────────────────────────────────────────────────────────
    auto* header = new QLabel(QString("EDIT %1 TRACKS").arg(tracks.size()), this);
    header->setObjectName("batchEditHeader");
    header->setStyleSheet(
        QString("font-family: '%1', '%2'; font-size: %3px; color: %4; "
                "letter-spacing: 3px; font-weight: bold;")
            .arg(Theme::Font::Mono, Theme::Font::MonoFallback)
            .arg(Theme::Font::Title)
            .arg(Theme::Color::Text));
    root->addWidget(header);

    // ── Form ────────────────────────────────────────────────────────────────
    auto* form = new QFormLayout;
    form->setSpacing(Theme::Layout::GapSm);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    auto makeEdit = [this](const QString& placeholder) -> QLineEdit* {
        auto* edit = new QLineEdit(this);
        edit->setPlaceholderText(placeholder);
        edit->setStyleSheet(
            QString("QLineEdit { background: %1; border: 1px solid %2; color: %3; "
                    "padding: %4px %5px; font-size: %6px; }")
                .arg(Theme::Color::Bg2, Theme::Color::Border, Theme::Color::Text)
                .arg(Theme::Layout::InputPadV).arg(Theme::Layout::InputPadH)
                .arg(Theme::Font::Body));
        return edit;
    };

    auto makeLabel = [this](const QString& text) -> QLabel* {
        auto* lbl = new QLabel(text, this);
        lbl->setStyleSheet(
            QString("color: %1; font-size: %2px;")
                .arg(Theme::Color::Text2).arg(Theme::Font::Secondary));
        return lbl;
    };

    m_artistEdit  = makeEdit("(keep existing)");
    m_albumEdit   = makeEdit("(keep existing)");
    m_genreEdit   = makeEdit("(keep existing)");
    m_bpmEdit     = makeEdit("(keep existing)");
    m_commentEdit = makeEdit("(keep existing)");

    form->addRow(makeLabel("Artist"),  m_artistEdit);
    form->addRow(makeLabel("Album"),   m_albumEdit);
    form->addRow(makeLabel("Genre"),   m_genreEdit);
    form->addRow(makeLabel("BPM"),     m_bpmEdit);
    form->addRow(makeLabel("Comment"), m_commentEdit);

    root->addLayout(form);

    // ── Rating row ──────────────────────────────────────────────────────────
    auto* ratingRow = new QHBoxLayout;
    ratingRow->setSpacing(Theme::Layout::GapXs);
    auto* ratingLabel = makeLabel("Rating");
    ratingLabel->setFixedWidth(60);
    ratingLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    ratingRow->addWidget(ratingLabel);

    for (int i = 0; i < 5; ++i) {
        auto* star = new QPushButton(QString::fromUtf8("\xe2\x98\x85"), this);
        star->setObjectName("batchRatingBtn");
        star->setFixedSize(28, 28);
        star->setCursor(Qt::PointingHandCursor);
        star->setProperty("active", false);
        star->setStyleSheet(
            QString("QPushButton { border: none; font-size: 16px; color: %1; background: transparent; }"
                    "QPushButton[active=\"true\"] { color: %2; }")
                .arg(Theme::Color::Text3, Theme::Color::Gold));
        connect(star, &QPushButton::clicked, this, [this, i]() {
            const int clicked = i + 1;
            m_rating = (m_rating == clicked) ? 0 : clicked;
            for (int j = 0; j < m_starBtns.size(); ++j) {
                m_starBtns[j]->setProperty("active", j < m_rating);
                m_starBtns[j]->style()->unpolish(m_starBtns[j]);
                m_starBtns[j]->style()->polish(m_starBtns[j]);
            }
        });
        m_starBtns.append(star);
        ratingRow->addWidget(star);
    }
    ratingRow->addStretch();
    root->addLayout(ratingRow);

    // ── Hint ────────────────────────────────────────────────────────────────
    auto* hint = new QLabel("Leave blank to keep existing value", this);
    hint->setObjectName("batchEditHint");
    hint->setStyleSheet(
        QString("color: %1; font-size: %2px; font-style: italic;")
            .arg(Theme::Color::Text3).arg(Theme::Font::Small));
    root->addWidget(hint);

    root->addStretch();

    // ── Footer ──────────────────────────────────────────────────────────────
    auto* footer = new QHBoxLayout;
    footer->setSpacing(Theme::Layout::GapSm);
    footer->addStretch();

    auto* cancelBtn = new QPushButton("CANCEL", this);
    cancelBtn->setObjectName("batchEditCancelBtn");
    cancelBtn->setCursor(Qt::PointingHandCursor);
    cancelBtn->setStyleSheet(
        QString("QPushButton { background: %1; border: 1px solid %2; color: %3; "
                "padding: %4px %5px; font-size: %6px; font-family: '%7', '%8'; "
                "letter-spacing: 1px; }"
                "QPushButton:hover { border-color: %9; }")
            .arg(Theme::Color::Bg2, Theme::Color::Border, Theme::Color::Text2)
            .arg(Theme::Layout::ButtonPadV).arg(Theme::Layout::ButtonPadH)
            .arg(Theme::Font::Secondary)
            .arg(Theme::Font::Mono, Theme::Font::MonoFallback)
            .arg(Theme::Color::BorderHov));
    connect(cancelBtn, &QPushButton::clicked, this, &BatchEditDialog::onCancelClicked);
    footer->addWidget(cancelBtn);

    auto* applyBtn = new QPushButton("APPLY", this);
    applyBtn->setObjectName("batchEditApplyBtn");
    applyBtn->setProperty("btnStyle", "accent");
    applyBtn->setCursor(Qt::PointingHandCursor);
    applyBtn->setStyleSheet(
        QString("QPushButton { background: %1; border: none; color: %2; "
                "padding: %3px %4px; font-size: %5px; font-family: '%6', '%7'; "
                "letter-spacing: 1px; font-weight: bold; }"
                "QPushButton:hover { background: %8; }")
            .arg(Theme::Color::Accent, Theme::Color::Bg)
            .arg(Theme::Layout::ButtonPadV).arg(Theme::Layout::ButtonPadH)
            .arg(Theme::Font::Secondary)
            .arg(Theme::Font::Mono, Theme::Font::MonoFallback)
            .arg(Theme::Color::RowSelHov));
    connect(applyBtn, &QPushButton::clicked, this, &BatchEditDialog::onApplyClicked);
    footer->addWidget(applyBtn);

    root->addLayout(footer);
}

void BatchEditDialog::onApplyClicked()
{
    const QString artist  = m_artistEdit->text().trimmed();
    const QString album   = m_albumEdit->text().trimmed();
    const QString genre   = m_genreEdit->text().trimmed();
    const QString bpmText = m_bpmEdit->text().trimmed();
    const QString comment = m_commentEdit->text().trimmed();

    for (const Track& orig : m_tracks) {
        Track t = orig;
        bool changed = false;

        if (!artist.isEmpty())  { t.artist  = artist.toStdString();  changed = true; }
        if (!album.isEmpty())   { t.album   = album.toStdString();   changed = true; }
        if (!genre.isEmpty())   { t.genre   = genre.toStdString();   changed = true; }
        if (!comment.isEmpty()) { t.comment = comment.toStdString(); changed = true; }

        if (!bpmText.isEmpty()) {
            bool ok = false;
            const double bpm = bpmText.toDouble(&ok);
            if (ok) { t.bpm = bpm; changed = true; }
        }

        if (m_rating > 0) { t.rating = m_rating; changed = true; }

        if (changed)
            m_db->updateSongMetadata(t.id, t);
    }

    emit applied();
    accept();
}

void BatchEditDialog::onCancelClicked()
{
    reject();
}
