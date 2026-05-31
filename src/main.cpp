#include "config/AppConfig.hpp"
#include "ui/ApiKeyDialog.hpp"
#include "ui/MainWindow.hpp"

#include <oclero/qlementine/style/QlementineStyle.hpp>
#include <QApplication>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("premiumize-explorer");
    app.setApplicationVersion("0.1.0");
    app.setOrganizationName("premiumize-explorer");

    auto* style = new oclero::qlementine::QlementineStyle(&app);
    QApplication::setStyle(style);

    if (!AppConfig::instance().isConfigured()) {
        ApiKeyDialog dlg;
        if (dlg.exec() != QDialog::Accepted) {
            return 0;
        }
        AppConfig::instance().setApiKey(dlg.apiKey());
    }

    MainWindow w;
    w.show();
    return app.exec();
}
