/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/browser/extensions/api/gemini_api.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "brave/browser/gemini/gemini_service_factory.h"
#include "brave/common/extensions/api/gemini.h"
#include "brave/components/brave_wallet/common/common_util.h"
#include "brave/components/gemini/browser/gemini_service.h"
#include "brave/components/gemini/browser/regions.h"
#include "brave/components/ntp_widget_utils/browser/ntp_widget_utils_region.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/constants.h"

namespace {

GeminiService* GetGeminiService(content::BrowserContext* context) {
  return GeminiServiceFactory::GetInstance()
      ->GetForProfile(Profile::FromBrowserContext(context));
}

}  // namespace

namespace extensions {
namespace api {

ExtensionFunction::ResponseAction
GeminiGetClientUrlFunction::Run() {
  auto* service = GetGeminiService(browser_context());
  const std::string client_url = service->GetOAuthClientUrl();

  return RespondNow(OneArgument(base::Value(client_url)));
}

ExtensionFunction::ResponseAction
GeminiGetAccessTokenFunction::Run() {
  auto* service = GetGeminiService(browser_context());
  bool token_request = service->GetAccessToken(base::BindOnce(
      &GeminiGetAccessTokenFunction::OnCodeResult, this));

  if (!token_request) {
    return RespondNow(
        Error("Could not make request for access tokens"));
  }

  return RespondLater();
}

void GeminiGetAccessTokenFunction::OnCodeResult(bool success) {
  Respond(OneArgument(base::Value(success)));
}

ExtensionFunction::ResponseAction
GeminiRefreshAccessTokenFunction::Run() {
  auto* service = GetGeminiService(browser_context());
  bool token_request = service->RefreshAccessToken(base::BindOnce(
      &GeminiRefreshAccessTokenFunction::OnRefreshResult, this));

  if (!token_request) {
    return RespondNow(
        Error("Could not make request to refresh access tokens"));
  }

  return RespondLater();
}

void GeminiRefreshAccessTokenFunction::OnRefreshResult(bool success) {
  Respond(OneArgument(base::Value(success)));
}

ExtensionFunction::ResponseAction
GeminiGetTickerPriceFunction::Run() {
  std::unique_ptr<gemini::GetTickerPrice::Params> params(
      gemini::GetTickerPrice::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  auto* service = GetGeminiService(browser_context());
  bool price_request = service->GetTickerPrice(
      params->asset,
      base::BindOnce(
          &GeminiGetTickerPriceFunction::OnPriceResult, this));

  if (!price_request) {
    return RespondNow(
        Error("Could not make request for price"));
  }

  return RespondLater();
}

void GeminiGetTickerPriceFunction::OnPriceResult(
    const std::string& price) {
  Respond(OneArgument(base::Value(price)));
}

ExtensionFunction::ResponseAction
GeminiGetAccountBalancesFunction::Run() {
  auto* service = GetGeminiService(browser_context());
  bool balance_success = service->GetAccountBalances(
      base::BindOnce(
          &GeminiGetAccountBalancesFunction::OnGetAccountBalances,
          this));

  if (!balance_success) {
    return RespondNow(Error("Could not send request to get balance"));
  }

  return RespondLater();
}

void GeminiGetAccountBalancesFunction::OnGetAccountBalances(
    const GeminiAccountBalances& balances,
    bool auth_invalid) {
  base::Value::Dict result;

  for (const auto& balance : balances) {
    result.Set(balance.first, balance.second);
  }

  Respond(
      TwoArguments(base::Value(std::move(result)), base::Value(auth_invalid)));
}

ExtensionFunction::ResponseAction
GeminiGetDepositInfoFunction::Run() {
  std::unique_ptr<gemini::GetDepositInfo::Params> params(
      gemini::GetDepositInfo::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  auto* service = GetGeminiService(browser_context());
  bool info_request = service->GetDepositInfo(params->asset,
      base::BindOnce(
          &GeminiGetDepositInfoFunction::OnGetDepositInfo, this));

  if (!info_request) {
    return RespondNow(
        Error("Could not make request for deposit information."));
  }

  return RespondLater();
}

void GeminiGetDepositInfoFunction::OnGetDepositInfo(
    const std::string& deposit_address) {
  Respond(OneArgument(base::Value(deposit_address)));
}

ExtensionFunction::ResponseAction
GeminiRevokeTokenFunction::Run() {
  auto* service = GetGeminiService(browser_context());
  bool request = service->RevokeAccessToken(base::BindOnce(
          &GeminiRevokeTokenFunction::OnRevokeToken, this));

  if (!request) {
    return RespondNow(
        Error("Could not revoke gemini access tokens"));
  }

  return RespondLater();
}

void GeminiRevokeTokenFunction::OnRevokeToken(bool success) {
  Respond(OneArgument(base::Value(success)));
}

ExtensionFunction::ResponseAction
GeminiGetOrderQuoteFunction::Run() {
  std::unique_ptr<gemini::GetOrderQuote::Params> params(
      gemini::GetOrderQuote::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  auto* service = GetGeminiService(browser_context());
  bool quote_request = service->GetOrderQuote(
      params->side, params->symbol, params->spend,
      base::BindOnce(
          &GeminiGetOrderQuoteFunction::OnOrderQuoteResult, this));

  if (!quote_request) {
    return RespondNow(
        Error("Could not make request for quote"));
  }

  return RespondLater();
}

void GeminiGetOrderQuoteFunction::OnOrderQuoteResult(
    const std::string& quote_id, const std::string& quantity,
    const std::string& fee, const std::string& price,
    const std::string& total_price, const std::string& error) {
  base::Value::Dict quote;
  quote.Set("id", quote_id);
  quote.Set("quantity", quantity);
  quote.Set("fee", fee);
  quote.Set("price", price);
  quote.Set("totalPrice", total_price);
  Respond(TwoArguments(base::Value(std::move(quote)), base::Value(error)));
}

ExtensionFunction::ResponseAction
GeminiExecuteOrderFunction::Run() {
  std::unique_ptr<gemini::ExecuteOrder::Params> params(
      gemini::ExecuteOrder::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  auto* service = GetGeminiService(browser_context());
  bool balance_success = service->ExecuteOrder(
      params->symbol, params->side, params->quantity,
      params->price, params->fee, params->quote_id,
      base::BindOnce(
          &GeminiExecuteOrderFunction::OnOrderExecuted,
          this));

  if (!balance_success) {
    return RespondNow(Error("Could not send request to execute order"));
  }

  return RespondLater();
}

void GeminiExecuteOrderFunction::OnOrderExecuted(bool success) {
  Respond(OneArgument(base::Value(success)));
}

ExtensionFunction::ResponseAction
GeminiIsSupportedFunction::Run() {
  Profile* profile = Profile::FromBrowserContext(browser_context());
  bool is_supported =
      ntp_widget_utils::IsRegionSupported(profile->GetPrefs(),
                                          ::gemini::supported_regions, true) &&
      brave_wallet::IsAllowed(profile->GetPrefs());
  return RespondNow(OneArgument(base::Value(is_supported)));
}

}  // namespace api
}  // namespace extensions
