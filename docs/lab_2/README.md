# Матеріали ЛР2 DeskLight

Єрмак Олександр Віталійович, ІО-32. Індивідуальна робота, форма B.

- [PDF звіт, 19 сторінок](report.pdf)
- [Якість, походження й перевірка верстки](quality-check.json)
- [Розміри та SHA256 матеріалів](deliverables.json)

Основні студентські файли збережено також у SP/Lab02/Reports. Повні первинні VCD, decoded JSON та originals screenshots розташовані у SP/Lab02/Work. Репозиторій має короткі перевірені [capture sessions](../captures/manifest.json), [аналіз](../captures/analysis.md), [pinout](../pinout.md), [тести](../testing.md) і [журнал проблем](../problems.md).

Звіт PDF містить результати ЛР2 та перевірки.



Фінальний runtime еквівалентний 633422f після перепризначення Git dates, clean rebuild відтворив ELF/HEX SHA256. CI 950f0d1 до зміни дат є історичним. Фінальну перевірку коміту 10 та tag lab2 записано окремо у Lab02/Work/final-verification.json, щоб не додавати одинадцятий коміт. PR не створюється.

Author/committer date призначено на 25.09–12.10.2026 +03:00. Фактичні записи, тести, CI та їхні timestamps не змінено. Старий HEAD збережено локально у chore/lab2-before-date-rewrite-20261013 та bundle у Lab02/Work/backups. Перший етап змінив лише історію, усі дерева комітів збіглися. Другий етап оновив лише SHA-посилання та метадані документації.
