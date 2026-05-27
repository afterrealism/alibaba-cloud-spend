# Alibaba Cloud Spend

GTK 4 dashboard for monitoring Alibaba Cloud billing — outstanding balances, monthly spend by service, AI/LLM model costs broken down by region, and 6-month historical data. Refreshes automatically every 60 seconds.

## Features

| Tab | Shows |
|-----|-------|
| **Overview** | Outstanding balance & monthly spend per service |
| **AI · Beijing** | LLM model costs in China (Beijing) region |
| **AI · Singapore** | LLM model costs in Singapore region |
| **History** | 6 months of cost history grouped by month |

- **Click any AI model** to see a billing-type breakdown (input tokens, output tokens, cache, etc.) with costs per component.
- **Auto-refreshes** every 60 seconds via background threads — no blocking UI.
- **Pagination** — fetches all billing entries using `NextToken`.
- **Dark theme** with Alibaba-branded orange accents.

## Requirements

- Linux with X11/Wayland
- **GTK 4** (development headers)
- **libcurl** (dev)
- **json-c** (dev)
- **OpenSSL** / **libcrypto** (dev)
- **GCC** or compatible C compiler

```bash
# Ubuntu / Debian
sudo apt install libgtk-4-dev libcurl4-openssl-dev libjson-c-dev libssl-dev
```

## Build

```bash
make
```

## Run

Set your Alibaba Cloud AccessKey credentials as environment variables, then run:

```bash
export ALIBABA_CLOUD_ACCESS_KEY_ID="your-access-key-id"
export ALIBABA_CLOUD_ACCESS_KEY_SECRET="your-access-key-secret"
./alibaba-cloud-spend
```

Your IAM user needs at least the `AliyunBSSReadOnlyAccess` policy.

## Project Structure

```
├── main.c        Entry point — GTK app initialization
├── api.h / api.c Alibaba Cloud BSS OpenAPI: signing, HTTP, JSON parsing, threads
├── ui.h  / ui.c  GTK 4 UI: cards, tabs, CSS styling, model detail views
├── config.h      Credentials (from env), API endpoint, constants
├── types.h       Shared data structures
└── Makefile      Build configuration
```

## API Used

- **[BssOpenApi 2017-12-14](https://www.alibabacloud.com/help/en/bss-openapi/)** — Billing & cost management
  - `QueryBillOverview` — Outstanding amounts
  - `DescribeInstanceBill` — Per-instance billing (services, regions, AI models)
  - `QueryBillOverview` (historical) — Multi-month history

API calls are **free** (not metered). Rate limit is ~10 req/sec per account; this app makes <2 req/min.

## License

MIT — see [LICENSE](LICENSE)