#pragma once

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLTexture>
#include <QOpenGLShaderProgram>

class RenderWidget : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT

public:
    explicit RenderWidget(QWidget* parent = nullptr);
    ~RenderWidget();

    void present(const uint32_t* framebuffer);
    void setScale(int scale);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;

private:
    void createTexture();
    void updateTexture();

    const uint32_t* framebuffer_ = nullptr;
    QOpenGLTexture* texture_ = nullptr;
    QOpenGLShaderProgram* program_ = nullptr;
    int scale_ = 3;
    int texWidth_ = 256;
    int texHeight_ = 240;
};
