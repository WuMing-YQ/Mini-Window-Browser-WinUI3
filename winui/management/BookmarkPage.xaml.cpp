#include "pch.h"
#include "BookmarkPage.xaml.h"
#include "BookmarkPage.g.cpp"
#include "UIHelpers.h"
namespace winrt::MiniWindow::implementation {
BookmarkPage::BookmarkPage(){InitializeComponent();
 Items().SelectionChanged([this](auto&&,auto&&){if(!syncing_&&!pending_)SelectAsync(Items().SelectedIndex());});
 Items().DoubleTapped([this](auto&&,auto&&){if(selected_&&!Dirty())app_->OpenBookmark(*selected_);});
 NewButton().Click([this](auto&&,auto&&){if(!pending_)SelectAsync(-1);});
 SaveButton().Click([this](auto&&,auto&&){Save();});OpenButton().Click([this](auto&&,auto&&){if(selected_&&!Dirty())app_->OpenBookmark(*selected_);});
 DeleteButton().Click([this](auto&&,auto&&){if(!pending_)DeleteAsync();});
}
void BookmarkPage::Bind(miniwindow::AppCoordinator& app){app_=&app;Refresh();}
bool BookmarkPage::Dirty(){return std::wstring(BookmarkTitle().Text())!=original_.title||std::wstring(BookmarkUrl().Text())!=original_.url;}
void BookmarkPage::Refresh(){if(!app_||Dirty())return;syncing_=true;Items().Items().Clear();for(const auto& bookmark:app_->Snapshot().state.bookmarks){auto text=miniwindow::ui::Text(bookmark.title);text.TextWrapping(Microsoft::UI::Xaml::TextWrapping::NoWrap);text.TextTrimming(Microsoft::UI::Xaml::TextTrimming::CharacterEllipsis);Microsoft::UI::Xaml::Controls::ToolTipService::SetToolTip(text,box_value(bookmark.title+L"\n"+bookmark.url));Items().Items().Append(text);}if(Items().Items().Size()==0)Items().Header(box_value(L"暂无书签"));else Items().Header(nullptr);if(selected_&&*selected_>=Items().Items().Size())selected_.reset();Items().SelectedIndex(selected_?static_cast<int>(*selected_):-1);syncing_=false;Load();}
void BookmarkPage::Load(){original_={};if(selected_)original_=app_->Snapshot().state.bookmarks[*selected_];BookmarkTitle().Text(original_.title);BookmarkUrl().Text(original_.url);OpenButton().IsEnabled(selected_.has_value());DeleteButton().IsEnabled(selected_.has_value());Error().Text(L"");}
bool BookmarkPage::Save(){if(!app_)return false;auto result=app_->SaveBookmark(selected_,{std::wstring(BookmarkTitle().Text()),std::wstring(BookmarkUrl().Text())});if(!result.ok){Error().Text(result.message);return false;}if(!selected_)selected_=app_->Snapshot().state.bookmarks.size()-1;original_={std::wstring(BookmarkTitle().Text()),std::wstring(BookmarkUrl().Text())};Refresh();return true;}
Windows::Foundation::IAsyncOperation<bool> BookmarkPage::CanLeaveAsync(){auto lifetime=get_strong();if(!Dirty())co_return true;Microsoft::UI::Xaml::Controls::ContentDialog dialog;dialog.XamlRoot(XamlRoot());dialog.Title(box_value(L"书签尚未保存"));dialog.Content(box_value(L"保存修改后继续，或放弃这次编辑。"));dialog.PrimaryButtonText(L"保存并继续");dialog.SecondaryButtonText(L"放弃修改");dialog.CloseButtonText(L"继续编辑");auto choice=co_await dialog.ShowAsync();if(choice==Microsoft::UI::Xaml::Controls::ContentDialogResult::Primary)co_return Save();if(choice==Microsoft::UI::Xaml::Controls::ContentDialogResult::Secondary){Load();co_return true;}co_return false;}
Windows::Foundation::IAsyncAction BookmarkPage::SelectAsync(int index){auto lifetime=get_strong();pending_=true;syncing_=true;Items().SelectedIndex(selected_?static_cast<int>(*selected_):-1);syncing_=false;try{if(co_await CanLeaveAsync()){selected_=index>=0?std::optional<size_t>(static_cast<size_t>(index)):std::nullopt;Load();syncing_=true;Items().SelectedIndex(index);syncing_=false;}}catch(const hresult_error& e){Error().Text(e.message());}pending_=false;}
Windows::Foundation::IAsyncAction BookmarkPage::DeleteAsync(){auto lifetime=get_strong();if(!selected_)co_return;pending_=true;try{if(co_await miniwindow::ui::Confirm(XamlRoot(),L"删除书签？",L"仅删除所选收藏，网页会继续保留。",L"删除")){auto result=app_->DeleteBookmark(*selected_);if(result.ok){selected_.reset();Load();Refresh();}else Error().Text(result.message);}}catch(const hresult_error& e){Error().Text(e.message());}pending_=false;}
}
