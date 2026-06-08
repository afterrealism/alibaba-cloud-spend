# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.1.0] - 2026-01-20

### Added
- **Proxy Dashboard**: New proxy monitoring feature with real-time usage tracking
  - Cost by model doughnut chart with legend
  - Daily cost bar chart
  - Recent requests list with clickable detail view
  - Request detail modal showing token breakdown and cost analysis
  - Load more functionality for request history
- Chart legends for better data visualization
- Back button styling for improved navigation

### Changed
- Combined Beijing and Singapore AI Models into single tab for cleaner interface
- Increased all text sizes throughout the UI for better readability
- Improved text color contrast (TEXT_SECONDARY and TEXT_MUTED)
- Enhanced chart rendering with proper color cycling

### Fixed
- Chart data lifetime bug causing use-after-free errors
- Chart iteration bounds now properly use count fields
- Fixed text color visibility issues

## [1.0.4] - Previous Release

### Added
- Token tracking and SQLite persistence
- Usage charts for model consumption

### Fixed
- Coupon API field names and error handling
- Billing display when coupons are applied
- Settings buttons styling
- Removed broken coupon feature
