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
void BookmarkPage::Refresh(){
 if(!app_)return;const bool dirty=Dirty();if(selectedId_)selected_=app_->BookmarkIndex(*selectedId_);
 syncing_=true;Items().Items().Clear();for(const auto& bookmark:app_->Snapshot().state.bookmarks){auto text=miniwindow::ui::Text(bookmark.title);text.TextWrapping(Microsoft::UI::Xaml::TextWrapping::NoWrap);text.TextTrimming(Microsoft::UI::Xaml::TextTrimming::CharacterEllipsis);Microsoft::UI::Xaml::Controls::ToolTipService::SetToolTip(text,box_value(bookmark.title+L"\n"+bookmark.url));Items().Items().Append(text);}
 if(Items().Items().Size()==0)Items().Header(box_value(L"暂无书签"));else Items().Header(nullptr);
 Items().SelectedIndex(selected_?static_cast<int>(*selected_):-1);syncing_=false;
 if(!dirty)Load();else if(selectedId_&&!selected_)Error().Text(L"正在编辑的书签已被删除，草稿已保留。可复制内容后新建，或放弃修改。");
}
void BookmarkPage::Load(){
 original_={};selected_=selectedId_?app_->BookmarkIndex(*selectedId_):std::nullopt;
 if(selected_&&*selected_<app_->Snapshot().state.bookmarks.size())original_=app_->Snapshot().state.bookmarks[*selected_];else{selected_.reset();selectedId_.reset();}
 BookmarkTitle().Text(original_.title);BookmarkUrl().Text(original_.url);OpenButton().IsEnabled(selected_.has_value());DeleteButton().IsEnabled(selected_.has_value());Error().Text(L"");
}
bool BookmarkPage::Save(){
 if(!app_)return false;selected_=selectedId_?app_->BookmarkIndex(*selectedId_):std::nullopt;
 if(selectedId_&&!selected_){Error().Text(L"原书签已删除，不能将草稿覆盖到其他书签。请复制内容后新建。");return false;}
 if(selected_){const auto& current=app_->Snapshot().state.bookmarks[*selected_];if(current.title!=original_.title||current.url!=original_.url){Error().Text(L"此书签已在其他位置修改，请保留草稿后重新选择。");return false;}}
 auto result=app_->SaveBookmark(selected_,{std::wstring(BookmarkTitle().Text()),std::wstring(BookmarkUrl().Text())});if(!result.ok){Error().Text(result.message);return false;}
 if(!selectedId_){selected_=app_->Snapshot().state.bookmarks.size()-1;selectedId_=app_->Snapshot().bookmarkIds[*selected_];}
 original_=app_->Snapshot().state.bookmarks[*selected_];Load();Refresh();return true;
}
Windows::Foundation::IAsyncOperation<bool> BookmarkPage::CanLeaveAsync(){auto lifetime=get_strong();if(!Dirty())co_return true;Microsoft::UI::Xaml::Controls::ContentDialog dialog;dialog.XamlRoot(XamlRoot());dialog.Title(box_value(L"书签尚未保存"));dialog.Content(box_value(L"保存修改后继续，或放弃这次编辑。"));dialog.PrimaryButtonText(L"保存并继续");dialog.SecondaryButtonText(L"放弃修改");dialog.CloseButtonText(L"继续编辑");auto choice=co_await dialog.ShowAsync();if(choice==Microsoft::UI::Xaml::Controls::ContentDialogResult::Primary)co_return Save();if(choice==Microsoft::UI::Xaml::Controls::ContentDialogResult::Secondary){Load();co_return true;}co_return false;}
Windows::Foundation::IAsyncAction BookmarkPage::SelectAsync(int index){auto lifetime=get_strong();const auto target=index>=0&&static_cast<size_t>(index)<app_->Snapshot().bookmarkIds.size()?std::optional<uint64_t>(app_->Snapshot().bookmarkIds[static_cast<size_t>(index)]):std::nullopt;pending_=true;syncing_=true;Items().SelectedIndex(selected_?static_cast<int>(*selected_):-1);syncing_=false;try{if(co_await CanLeaveAsync()){selectedId_=target;Load();Refresh();}}catch(const hresult_error& e){Error().Text(e.message());}pending_=false;}
Windows::Foundation::IAsyncAction BookmarkPage::DeleteAsync(){auto lifetime=get_strong();if(!selected_)co_return;pending_=true;try{if(co_await miniwindow::ui::Confirm(XamlRoot(),L"删除书签？",L"仅删除所选收藏，网页会继续保留。",L"删除")){selected_=selectedId_?app_->BookmarkIndex(*selectedId_):std::nullopt;if(!selected_){Error().Text(L"书签已不存在。");pending_=false;co_return;}auto result=app_->DeleteBookmark(*selected_);if(result.ok){selected_.reset();selectedId_.reset();Load();Refresh();}else Error().Text(result.message);}}catch(const hresult_error& e){Error().Text(e.message());}pending_=false;}
}
